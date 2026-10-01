#include "IMUVisualizer.h"
#include <QVBoxLayout>
#include <QtCharts/QChart>

IMUVisualizer::IMUVisualizer(QWidget *p) : QWidget(p) {
    auto *chart = new QChart;
    chart->setTitle("惯性测量单元");
    chart->setTheme(QChart::ChartThemeLight);
    chart->setBackgroundRoundness(0);
    chart->setMargins(QMargins(4, 4, 4, 4));
    chart->legend()->setVisible(false);

    m_accX = new QLineSeries; m_accX->setName("加速度 X");
    m_accY = new QLineSeries; m_accY->setName("加速度 Y");
    m_accZ = new QLineSeries; m_accZ->setName("加速度 Z");
    m_gyrX = new QLineSeries; m_gyrX->setName("陀螺仪 X");
    m_gyrY = new QLineSeries; m_gyrY->setName("陀螺仪 Y");
    m_gyrZ = new QLineSeries; m_gyrZ->setName("陀螺仪 Z");

    chart->addSeries(m_accX); chart->addSeries(m_accY); chart->addSeries(m_accZ);
    chart->addSeries(m_gyrX); chart->addSeries(m_gyrY); chart->addSeries(m_gyrZ);

    m_timeAxis = new QValueAxis; m_timeAxis->setRange(0, 10); m_timeAxis->setTitleText("时间 (秒)");
    auto *valAxis = new QValueAxis; valAxis->setRange(-20, 20); valAxis->setTitleText("数值");
    m_valueAxis = valAxis;
    chart->addAxis(m_timeAxis, Qt::AlignBottom);
    chart->addAxis(valAxis, Qt::AlignLeft);
    for (auto *s : {m_accX, m_accY, m_accZ, m_gyrX, m_gyrY, m_gyrZ}) {
        s->attachAxis(m_timeAxis); s->attachAxis(valAxis);
    }

    m_chartView = new QChartView(chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
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
    m_chartView->chart()->setTitle(d.accelerometerInADC ? "加速度 (ADC) · X / Y / Z" : "加速度 (m/s²) · X / Y / Z");
    m_valueAxis->setRange(d.accelerometerInADC ? -2000 : -20, d.accelerometerInADC ? 2000 : 20);
    m_accX->append(m_elapsed, d.accelerometer.x);
    m_accY->append(m_elapsed, d.accelerometer.y);
    m_accZ->append(m_elapsed, d.accelerometer.z);
    if (d.has(SensorData::Gyroscope)) {
        m_gyrX->append(m_elapsed, d.gyroscope.x);
        m_gyrY->append(m_elapsed, d.gyroscope.y);
        m_gyrZ->append(m_elapsed, d.gyroscope.z);
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
