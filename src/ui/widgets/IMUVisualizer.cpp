#include "IMUVisualizer.h"
#include <QVBoxLayout>
#include <QGraphicsLayout>
#include <QtCharts/QLegend>
#include <QtCharts/QChart>

IMUVisualizer::IMUVisualizer(QWidget *p) : QWidget(p) {
    auto *chart = new QChart;
    // The panel is often under 200 px tall: no title, no axis titles, tight margins,
    // so the plot itself keeps most of the space. Units are shown in the legend.
    chart->setTheme(QChart::ChartThemeLight);
    chart->setBackgroundRoundness(0);
    chart->setMargins(QMargins(2, 2, 6, 2));
    chart->layout()->setContentsMargins(0, 0, 0, 0);

    m_accX = new QLineSeries; m_accX->setName("加速度 X");
    m_accY = new QLineSeries; m_accY->setName("加速度 Y");
    m_accZ = new QLineSeries; m_accZ->setName("加速度 Z");
    m_gyrX = new QLineSeries; m_gyrX->setName("陀螺仪 X");
    m_gyrY = new QLineSeries; m_gyrY->setName("陀螺仪 Y");
    m_gyrZ = new QLineSeries; m_gyrZ->setName("陀螺仪 Z");

    chart->addSeries(m_accX); chart->addSeries(m_accY); chart->addSeries(m_accZ);
    chart->addSeries(m_gyrX); chart->addSeries(m_gyrY); chart->addSeries(m_gyrZ);

    m_timeAxis = new QValueAxis; m_timeAxis->setRange(0, 10);
    m_timeAxis->setTickCount(6);
    auto *valAxis = new QValueAxis; valAxis->setRange(-20, 20);
    valAxis->setTickCount(5);
    m_valueAxis = valAxis;
    chart->addAxis(m_timeAxis, Qt::AlignBottom);
    chart->addAxis(valAxis, Qt::AlignLeft);
    for (auto *s : {m_accX, m_accY, m_accZ, m_gyrX, m_gyrY, m_gyrZ}) {
        s->attachAxis(m_timeAxis); s->attachAxis(valAxis);
    }

    m_chartView = new QChartView(chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    const char *colors[6] = {"#d74950", "#0f9d8a", "#3e7aab", "#c2557a", "#c08a1e", "#8b61a4"};
    int colorIndex = 0;
    for (auto *series : {m_accX, m_accY, m_accZ, m_gyrX, m_gyrY, m_gyrZ}) {
        series->setPen(QPen(QColor(colors[colorIndex++]), 1.6));
    }
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignTop);
    chart->legend()->setMarkerShape(QLegend::MarkerShapeCircle);
    QFont legendFont; legendFont.setPixelSize(10);
    chart->legend()->setFont(legendFont);
    chart->legend()->setContentsMargins(0, 0, 0, 0);
    for (auto *series : {m_gyrX, m_gyrY, m_gyrZ}) series->setVisible(false);
    QFont axisFont; axisFont.setPixelSize(10);
    m_timeAxis->setLabelsFont(axisFont); valAxis->setLabelsFont(axisFont);
    m_timeAxis->setLabelFormat("%.0f s"); valAxis->setLabelFormat("%.0f");
    m_timeAxis->setGridLineColor(QColor("#edf1f2")); valAxis->setGridLineColor(QColor("#edf1f2"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(m_chartView);
}

void IMUVisualizer::updateData(const SensorData &d) {
    const auto timestamp = d.fieldTimestamps[SensorData::Accelerometer];
    if (!d.has(SensorData::Accelerometer) || timestamp <= m_lastTimestamp) return;
    if (!m_origin) m_origin = timestamp;
    m_lastTimestamp = timestamp;
    m_elapsed = (timestamp - m_origin) / 1000.0;
    const QString unit = d.accelerometerInADC ? " (ADC)" : " (m/s²)";
    if (!m_accX->name().endsWith(unit)) {
        m_accX->setName("加速度 X" + unit);
        m_accY->setName("Y" + unit);
        m_accZ->setName("Z" + unit);
    }
    m_accX->append(m_elapsed, d.accelerometer.x);
    m_accY->append(m_elapsed, d.accelerometer.y);
    m_accZ->append(m_elapsed, d.accelerometer.z);
    if (d.has(SensorData::Gyroscope)) {
        if (!m_gyrX->isVisible()) for (auto *series : {m_gyrX, m_gyrY, m_gyrZ}) series->setVisible(true);
        m_gyrX->append(m_elapsed, d.gyroscope.x);
        m_gyrY->append(m_elapsed, d.gyroscope.y);
        m_gyrZ->append(m_elapsed, d.gyroscope.z);
    }
    // Fit the value axis to what is on screen. Raw ADC offsets differ between
    // firmware builds, so a fixed range can leave a channel off the chart.
    double low = 0, high = 0;
    bool any = false;
    for (auto *series : {m_accX, m_accY, m_accZ, m_gyrX, m_gyrY, m_gyrZ}) {
        const auto points = series->points();
        for (const auto &point : points) {
            if (point.x() < m_elapsed - 10) continue;
            low = any ? qMin(low, point.y()) : point.y();
            high = any ? qMax(high, point.y()) : point.y();
            any = true;
        }
    }
    if (any) {
        const double floorSpan = d.accelerometerInADC ? 200.0 : 2.0;
        const double span = qMax(floorSpan, high - low);
        const double mid = (high + low) / 2.0;
        const double wantLow = mid - span * 0.6, wantHigh = mid + span * 0.6;
        // Hysteresis: only move the axis when the data leaves it or it is far too
        // wide, otherwise the grid would jitter with every sample.
        const double curLow = m_valueAxis->min(), curHigh = m_valueAxis->max();
        if (low < curLow || high > curHigh || (curHigh - curLow) > (wantHigh - wantLow) * 2.5) {
            m_valueAxis->setRange(wantLow, wantHigh);
        }
    }
    if (m_elapsed > 10) {
        m_timeAxis->setRange(m_elapsed - 10, m_elapsed);
        for (auto *s : {m_accX, m_accY, m_accZ, m_gyrX, m_gyrY, m_gyrZ}) {
            int expired = 0;
            while (expired < s->count() && s->at(expired).x() < m_elapsed - 10) ++expired;
            if (expired) s->removePoints(0, expired);
        }
    }
}

void IMUVisualizer::reset() {
    m_origin = m_lastTimestamp = 0;
    m_elapsed = 0;
    m_timeAxis->setRange(0, 10);
    for (auto *series : {m_accX, m_accY, m_accZ, m_gyrX, m_gyrY, m_gyrZ}) series->clear();
}
