#pragma once
#include <QWidget>
#include "core/SensorData.h"
class MicrophoneVisualizer : public QWidget {
    Q_OBJECT
public: explicit MicrophoneVisualizer(QWidget *p = nullptr) : QWidget(p) {}
    void updateData(const SensorData &d) { m_data = d; update(); }
protected: void paintEvent(QPaintEvent *) override;
private: SensorData m_data;
};
