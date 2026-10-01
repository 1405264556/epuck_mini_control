#pragma once
#include <QWidget>
#include "core/Types.h"
#include "core/SensorData.h"
class QTabWidget;
class QTimer;
class QComboBox;
class QLabel;
class QTableWidget;
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
    void setMultiRobotMode(bool enabled);
public slots:
    void onSensorDataUpdated(const RobotId &id, const SensorData &data);
    void onSelectionChanged(const RobotId &id);
    void requestCameraFrame();
private:
    void refreshDisplay();
    void refreshRobotChoices();
    RobotManager *m_robotManager;
    QTabWidget *m_tabWidget;
    ProximityVisualizer *m_proxVis;
    IMUVisualizer *m_imuVis;
    ToFVisualizer *m_tofVis;
    MicrophoneVisualizer *m_micVis;
    CameraViewWidget *m_camVis;
    QTimer *m_cameraTimer = nullptr;
    QComboBox *m_robotCombo;
    QComboBox *m_sensorCombo;
    QLabel *m_status;
    QTableWidget *m_fleetTable;
    QTableWidget *m_values;
    QTimer *m_refreshTimer;
    SensorData m_pending;
    bool m_dirty = true;
    int m_statusTick = 0;
    RobotId m_currentId;
};
