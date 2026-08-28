#pragma once
#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include "core/SensorData.h"

class IMUVisualizer : public QWidget {
    Q_OBJECT
public:
    explicit IMUVisualizer(QWidget *p = nullptr);
    void updateData(const SensorData &d);
private:
    QChartView *m_chartView;
    QLineSeries *m_accX, *m_accY, *m_accZ;
    QLineSeries *m_gyrX, *m_gyrY, *m_gyrZ;
    QValueAxis *m_timeAxis;
    double m_elapsed = 0;
};
