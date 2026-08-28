#pragma once
#include <QLabel>
struct SensorData;
class CameraViewWidget : public QLabel {
    Q_OBJECT
public:
    explicit CameraViewWidget(QWidget *p = nullptr);
    void updateData(const SensorData &d);
};
