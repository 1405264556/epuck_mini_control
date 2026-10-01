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
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QDateTime>
#include <QHeaderView>
#include <QTableWidget>
#include <QSignalBlocker>
#include <algorithm>

SensorDisplayWidget::SensorDisplayWidget(RobotManager *robotMgr, QWidget *parent)
    : QWidget(parent), m_robotManager(robotMgr)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    m_fleetTable = new QTableWidget(0, 3);
    m_fleetTable->setObjectName("fleetSensorOverview");
    m_fleetTable->setHorizontalHeaderLabels({"设备", "反馈", "数据时效"});
    m_fleetTable->verticalHeader()->hide();
    m_fleetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_fleetTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fleetTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fleetTable->setMaximumHeight(160);
    m_fleetTable->hide();
    layout->addWidget(m_fleetTable);
    m_robotCombo = new QComboBox;
    m_robotCombo->setObjectName("sensorRobotSelector");
    m_robotCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_robotCombo->setMinimumContentsLength(10);
    layout->addWidget(m_robotCombo);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    m_sensorCombo = new QComboBox;
    m_sensorCombo->addItems({"接近传感器", "加速度 / IMU", "ToF 测距", "麦克风", "摄像头", "数据与时效"});
    m_sensorCombo->setObjectName("sensorTypeSelector");
    layout->addWidget(m_sensorCombo);
    m_tabWidget = new QTabWidget;
    m_tabWidget->tabBar()->hide();
    m_proxVis = new ProximityVisualizer;
    m_imuVis = new IMUVisualizer;
    m_imuVis->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    m_tofVis = new ToFVisualizer;
    m_micVis = new MicrophoneVisualizer;
    m_camVis = new CameraViewWidget;
    m_tabWidget->addTab(m_proxVis, "接近传感器");
    m_tabWidget->addTab(m_imuVis, "惯性测量");
    m_tabWidget->addTab(m_tofVis, "飞行时间");
    m_tabWidget->addTab(m_micVis, "麦克风");
    m_tabWidget->addTab(m_camVis, "摄像头");
    m_values = new QTableWidget(0, 3);
    m_values->setHorizontalHeaderLabels({"信号", "数值", "状态"});
    m_values->verticalHeader()->hide();
    m_values->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_values->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tabWidget->addTab(m_values, "数据");
    layout->addWidget(m_tabWidget, 1);
    connect(m_sensorCombo, &QComboBox::currentIndexChanged, m_tabWidget, &QTabWidget::setCurrentIndex);
    connect(m_robotCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_robotManager->setSelectedRobot(m_robotCombo->itemData(index).toString());
    });
    connect(m_fleetTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        m_robotManager->setSelectedRobot(m_fleetTable->item(row, 0)->data(Qt::UserRole).toString());
    });

    m_cameraTimer = new QTimer(this);
    m_cameraTimer->setInterval(500);
    connect(m_cameraTimer, &QTimer::timeout, this, &SensorDisplayWidget::requestCameraFrame);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        m_dirty = true;
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
    connect(m_robotManager, &RobotManager::robotAdded, this, [this](const RobotId &) { refreshRobotChoices(); });
    connect(m_robotManager, &RobotManager::robotRemoved, this, [this](const RobotId &) { refreshRobotChoices(); });
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(50);
    connect(m_refreshTimer, &QTimer::timeout, this, &SensorDisplayWidget::refreshDisplay);
    m_refreshTimer->start();
    refreshRobotChoices();
    onSelectionChanged(m_robotManager->selectedRobotId());
}

void SensorDisplayWidget::onSensorDataUpdated(const RobotId &id, const SensorData &data) {
    if (id != m_currentId) return;
    m_pending = data;
    m_dirty = true;
}

void SensorDisplayWidget::onSelectionChanged(const RobotId &id) {
    m_currentId = id;
    const QSignalBlocker blocker(m_robotCombo);
    m_robotCombo->setCurrentIndex(m_robotCombo->findData(id));
    m_pending.clear();
    m_imuVis->reset();
    if (auto *r = m_robotManager->robot(id)) {
        m_pending = r->latestSensorData();
    }
    m_dirty = true;
    m_camVis->updateData(m_pending);
    refreshDisplay();
}

void SensorDisplayWidget::requestCameraFrame() {
    if (m_currentId.isEmpty()) return;
    if (!isVisible()) return;
    if (m_tabWidget->currentWidget() != m_camVis) return;

    auto *robot = m_robotManager->robot(m_currentId);
    if (!robot || robot->state() != RobotConnectionState::Connected) return;

    if (auto *serial = robot->serialManager()) {
        serial->requestCameraFrame(m_currentId);
    }
}

void SensorDisplayWidget::setMultiRobotMode(bool enabled) { m_fleetTable->setVisible(enabled); }

void SensorDisplayWidget::refreshRobotChoices() {
    const QSignalBlocker blocker(m_robotCombo);
    m_robotCombo->clear();
    for (auto *robot : m_robotManager->allRobots()) {
        m_robotCombo->addItem(robot->deviceInfo().portName + " · " + robot->name(), robot->id());
    }
    m_robotCombo->setCurrentIndex(m_robotCombo->findData(m_robotManager->selectedRobotId()));
}

void SensorDisplayWidget::refreshDisplay() {
    if (!isVisible()) return;
    if (m_dirty) {
        switch (m_tabWidget->currentIndex()) {
        case 0: m_proxVis->updateData(m_pending); break;
        case 1: m_imuVis->updateData(m_pending); break;
        case 2: m_tofVis->updateData(m_pending); break;
        case 3: m_micVis->updateData(m_pending); break;
        case 4: m_camVis->updateData(m_pending); break;
        }
        m_dirty = false;
    }
    if (++m_statusTick % 5 != 0) return;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    auto *robot = m_robotManager->robot(m_currentId);
    const bool online = robot && robot->state() == RobotConnectionState::Connected;
    const auto age = m_pending.timestamp ? QString::number(qMax<qint64>(0, now - m_pending.timestamp)) + " ms" : "尚未收到";
    const SensorData::Field fields[] = {SensorData::Proximity, SensorData::Accelerometer,
        SensorData::ToF, SensorData::Microphone, SensorData::Camera, SensorData::Proximity};
    const auto field = fields[m_tabWidget->currentIndex()];
    const QString signalState = !m_pending.has(field) ? "未接入"
        : online && m_pending.isFresh(field, now, field == SensorData::Camera ? 2500 : 1000) ? "实时" : "过期";
    m_status->setText(QString("%1 · %2 · 最新反馈 %3").arg(online ? "在线" : "离线", signalState, age));
    const auto robots = m_robotManager->allRobots();
    m_fleetTable->setRowCount(robots.size());
    for (int row = 0; row < robots.size(); ++row) {
        auto *r = robots[row];
        const auto &d = r->latestSensorData();
        const bool connected = r->state() == RobotConnectionState::Connected;
        const QString state = !connected ? "离线" : d.isFresh(SensorData::Proximity, now) ? "实时" : "等待 / 过期";
        const QStringList values{r->deviceInfo().portName, state,
            d.timestamp ? QString::number(qMax<qint64>(0, now-d.timestamp)) + " ms" : "--"};
        for (int col = 0; col < 3; ++col) {
            if (!m_fleetTable->item(row, col)) m_fleetTable->setItem(row, col, new QTableWidgetItem);
            m_fleetTable->item(row, col)->setText(values[col]);
            m_fleetTable->item(row, col)->setData(Qt::UserRole, r->id());
        }
    }
    if (m_tabWidget->currentIndex() != 5) return;
    m_values->setRowCount(0);
    auto add = [&](const QString &name, const QString &value, SensorData::Field field) {
        const int row = m_values->rowCount(); m_values->insertRow(row);
        const QString state = !m_pending.has(field) ? "未接入"
            : !online || !m_pending.isFresh(field, now, field == SensorData::Camera ? 2500 : 1000)
                ? "过期" : "实时";
        m_values->setItem(row, 0, new QTableWidgetItem(name));
        m_values->setItem(row, 1, new QTableWidgetItem(m_pending.has(field) ? value : "--"));
        m_values->setItem(row, 2, new QTableWidgetItem(state));
    };
    for (int i = 0; i < 8; ++i) add(QString("IR %1").arg(i), QString::number(m_pending.proximity[i]), SensorData::Proximity);
    add("加速度 X", QString::number(m_pending.accelerometer.x, 'f', 1), SensorData::Accelerometer);
    add("加速度 Y", QString::number(m_pending.accelerometer.y, 'f', 1), SensorData::Accelerometer);
    add("加速度 Z", QString::number(m_pending.accelerometer.z, 'f', 1), SensorData::Accelerometer);
    add("加速度幅度", QString::number(m_pending.accelMagnitude, 'f', 1), SensorData::AccelSpherical);
    add("方向 (度)", QString::number(m_pending.accelOrientation, 'f', 1), SensorData::AccelSpherical);
    add("倾角 (度)", QString::number(m_pending.accelInclination, 'f', 1), SensorData::AccelSpherical);
    add("陀螺仪 Z", QString::number(m_pending.gyroscope.z, 'f', 1), SensorData::Gyroscope);
    for (int i = 0; i < m_pending.microphoneChannels; ++i) add(QString("麦克风 %1").arg(i+1),
        QString::number(m_pending.microphones[i], 'f', 3), SensorData::Microphone);
    add("左编码器", QString::number(m_pending.encoders[0]), SensorData::Encoders);
    add("右编码器", QString::number(m_pending.encoders[1]), SensorData::Encoders);
    add("ToF 最近 (mm)", QString::number(*std::min_element(m_pending.tof.begin(), m_pending.tof.end())), SensorData::ToF);
    add("电量 (%)", QString::number(m_pending.batteryPercent), SensorData::Battery);
    add("Selector", QString::number(m_pending.selectorPosition), SensorData::Selector);
    add("摄像头", QString("%1 × %2").arg(m_pending.cameraFrame.width()).arg(m_pending.cameraFrame.height()), SensorData::Camera);
}
