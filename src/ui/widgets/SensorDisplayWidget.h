#pragma once
#include <QWidget>
#include "core/Types.h"
#include "core/SensorData.h"
class QTabWidget;
class QTimer;
class RobotManager;
class ProximityVisualizer;
class IMUVisualizer;
class ToFVisualizer;
class MicrophoneVisualizer;
class CameraViewWidget;

class SensorDisplayWidget : public QWidget {
    Q_OBJECT
public:
    explicit SensorDisplayWidget(RobotManager *robotMgr, QWidget *parent = nullptr);
public slots:
    void onSensorDataUpdated(const RobotId &id, const SensorData &data);
    void onSelectionChanged(const RobotId &id);
    void requestCameraFrame();
private:
    RobotManager *m_robotManager;
    QTabWidget *m_tabWidget;
    ProximityVisualizer *m_proxVis;
    IMUVisualizer *m_imuVis;
    ToFVisualizer *m_tofVis;
    MicrophoneVisualizer *m_micVis;
    CameraViewWidget *m_camVis;
    QTimer *m_cameraTimer = nullptr;
    RobotId m_currentId;
};
