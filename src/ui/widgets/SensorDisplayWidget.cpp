#include "SensorDisplayWidget.h"
#include "ProximityVisualizer.h"
#include "IMUVisualizer.h"
#include "ToFVisualizer.h"
#include "MicrophoneVisualizer.h"
#include "CameraViewWidget.h"
#include "core/RobotManager.h"
#include "core/RobotInstance.h"
#include "comm/SerialManager.h"
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

SensorDisplayWidget::SensorDisplayWidget(RobotManager *robotMgr, QWidget *parent)
    : QWidget(parent), m_robotManager(robotMgr)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_tabWidget = new QTabWidget;
    m_proxVis = new ProximityVisualizer;
    m_imuVis = new IMUVisualizer;
    m_tofVis = new ToFVisualizer;
    m_micVis = new MicrophoneVisualizer;
    m_camVis = new CameraViewWidget;
    m_tabWidget->addTab(m_proxVis, "接近传感器");
    m_tabWidget->addTab(m_imuVis, "惯性测量");
    m_tabWidget->addTab(m_tofVis, "飞行时间");
    m_tabWidget->addTab(m_micVis, "麦克风");
    m_tabWidget->addTab(m_camVis, "摄像头");
    layout->addWidget(m_tabWidget);

    m_cameraTimer = new QTimer(this);
    m_cameraTimer->setInterval(500);
    connect(m_cameraTimer, &QTimer::timeout, this, &SensorDisplayWidget::requestCameraFrame);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        if (m_tabWidget->widget(index) == m_camVis) {
            m_cameraTimer->start();
            requestCameraFrame();
        } else {
            m_cameraTimer->stop();
        }
    });

    connect(m_robotManager, &RobotManager::robotSensorDataUpdated,
            this, &SensorDisplayWidget::onSensorDataUpdated);
    connect(m_robotManager, &RobotManager::selectedRobotChanged,
            this, &SensorDisplayWidget::onSelectionChanged);
}

void SensorDisplayWidget::onSensorDataUpdated(const RobotId &id, const SensorData &data) {
    if (id != m_currentId) return;
    m_proxVis->updateData(data);
    m_imuVis->updateData(data);
    m_tofVis->updateData(data);
    m_micVis->updateData(data);
    m_camVis->updateData(data);
}

void SensorDisplayWidget::onSelectionChanged(const RobotId &id) {
    m_currentId = id;
    if (auto *r = m_robotManager->robot(id)) {
        auto &d = r->latestSensorData();
        m_proxVis->updateData(d);
        m_imuVis->updateData(d);
        m_tofVis->updateData(d);
        m_micVis->updateData(d);
        m_camVis->updateData(d);
    }
}

void SensorDisplayWidget::requestCameraFrame() {
    if (m_currentId.isEmpty()) return;
    if (m_tabWidget->currentWidget() != m_camVis) return;

    auto *robot = m_robotManager->robot(m_currentId);
    if (!robot || robot->state() != RobotConnectionState::Connected) return;

    if (auto *serial = robot->serialManager()) {
        serial->requestCameraFrame(m_currentId);
    }
}
