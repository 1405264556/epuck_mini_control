#include "RobotListWidget.h"
#include "../models/RobotListModel.h"
#include "../delegates/RobotItemDelegate.h"
#include "core/RobotManager.h"
#include "core/RobotInstance.h"
#include "comm/SerialManager.h"
#include "comm/BLEDeviceInfo.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QListView>
#include <QItemSelectionModel>
#include <QHBoxLayout>
#include <QProgressBar>

RobotListWidget::RobotListWidget(RobotManager *robotMgr, SerialManager *serialMgr,
                                   QWidget *parent)
    : QWidget(parent), m_robotManager(robotMgr), m_serialManager(serialMgr)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_scanBtn = new QPushButton("后台扫描 COM 端口");
    m_connectBtn = new QPushButton("连接");
    m_disconnectBtn = new QPushButton("断开");
    m_connectAllBtn = new QPushButton("连接全部");
    m_disconnectAllBtn = new QPushButton("全部断开");
    m_scanProgress = new QProgressBar;
    m_scanProgress->setTextVisible(true);
    m_scanProgress->setVisible(false);
    m_connectBtn->setEnabled(false);
    m_disconnectBtn->setEnabled(false);
    m_connectAllBtn->setEnabled(false);
    m_disconnectAllBtn->setEnabled(false);

    layout->addWidget(m_scanBtn);
    layout->addWidget(m_scanProgress);
    auto *selectedButtons = new QHBoxLayout;
    selectedButtons->addWidget(m_connectBtn);
    selectedButtons->addWidget(m_disconnectBtn);
    layout->addLayout(selectedButtons);
    auto *allButtons = new QHBoxLayout;
    allButtons->addWidget(m_connectAllBtn);
    allButtons->addWidget(m_disconnectAllBtn);
    layout->addLayout(allButtons);

    m_model = new RobotListModel(m_robotManager, this);
    m_delegate = new RobotItemDelegate(this);
    m_listView = new QListView;
    m_listView->setModel(m_model);
    m_listView->setItemDelegate(m_delegate);
    layout->addWidget(m_listView);

    connect(m_scanBtn, &QPushButton::clicked, this, &RobotListWidget::onScanClicked);
    connect(m_connectBtn, &QPushButton::clicked, this, &RobotListWidget::onConnectClicked);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &RobotListWidget::onDisconnectClicked);
    connect(m_connectAllBtn, &QPushButton::clicked, this, [this]() {
        QList<RobotId> ids;
        for (auto *robot : m_robotManager->allRobots()) {
            if (robot->state() == RobotConnectionState::Disconnected
                || robot->state() == RobotConnectionState::Error) {
                ids.append(robot->id());
            }
        }
        if (!ids.isEmpty()) emit connectAllRequested(ids);
    });
    connect(m_disconnectAllBtn, &QPushButton::clicked, this, [this]() {
        QList<RobotId> ids;
        for (auto *robot : m_robotManager->allRobots()) {
            if (robot->state() == RobotConnectionState::Connected || robot->state() == RobotConnectionState::Connecting)
                ids.append(robot->id());
        }
        if (!ids.isEmpty()) emit disconnectAllRequested(ids);
    });
    connect(m_listView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this]() { onSelectionChanged(); });
    connect(m_serialManager, &SerialManager::deviceDiscovered, this,
            [this](const BLEDeviceInfo &info) {
                if (!info.isEpuckMini || info.portName.isEmpty()) return;

                auto *robot = m_robotManager->addRobot(info);
                robot->setSerialManager(m_serialManager);

                if (m_selectedId.isEmpty()) {
                    const RobotId id = robot->id();
                    m_robotManager->setSelectedRobot(id);
                    for (int row = 0; row < m_model->rowCount({}); ++row) {
                        if (m_model->robotIdAt(row) == id) {
                            m_listView->setCurrentIndex(m_model->index(row, 0));
                            break;
                        }
                    }
                }
                refreshButtons();
            });
    connect(m_robotManager, &RobotManager::robotConnectionStateChanged, this,
            [this](const RobotId &id, RobotConnectionState) {
                if (id == m_selectedId) onSelectionChanged();
                refreshButtons();
            });
    connect(m_serialManager, &SerialManager::scanStarted, this, [this]() {
        m_scanBtn->setEnabled(true);
        m_scanBtn->setText("停止扫描");
        m_scanProgress->setRange(0, 0);
        m_scanProgress->setFormat("正在枚举端口...");
        m_scanProgress->setVisible(true);
    });
    connect(m_serialManager, &SerialManager::scanProgress, this,
            [this](int completed, int total, const QString &portName, bool verified) {
        m_scanProgress->setRange(0, qMax(1, total));
        m_scanProgress->setValue(completed);
        m_scanProgress->setFormat(QString("%1/%2  %3  %4")
            .arg(completed).arg(total).arg(portName, verified ? "已识别" : "无响应"));
    });
    connect(m_serialManager, &SerialManager::scanFinished, this, [this]() {
        m_scanBtn->setEnabled(true);
        m_scanBtn->setText("扫描 COM 端口");
        m_scanProgress->setVisible(false);
        refreshButtons();
    });
    connect(m_robotManager, &RobotManager::selectedRobotChanged, this, [this](const RobotId &id) {
        for (int row = 0; row < m_model->rowCount({}); ++row) {
            if (m_model->robotIdAt(row) == id) { m_listView->setCurrentIndex(m_model->index(row, 0)); break; }
        }
    });
    if (m_model->rowCount({}) > 0) m_listView->setCurrentIndex(m_model->index(0, 0));
    refreshButtons();
}

void RobotListWidget::onScanClicked() {
    if (m_serialManager->isScanning()) { m_serialManager->stopScan(); return; }
    m_serialManager->scanPorts();
}

void RobotListWidget::onConnectClicked() {
    if (!m_selectedId.isEmpty()) {
        m_connectBtn->setEnabled(false);
        emit connectRequested(m_selectedId);
    }
}

void RobotListWidget::onDisconnectClicked() {
    if (!m_selectedId.isEmpty()) {
        m_serialManager->disconnectRobot(m_selectedId);
        emit disconnectRequested(m_selectedId);
    }
}

void RobotListWidget::onSelectionChanged() {
    auto idx = m_listView->currentIndex();
    if (idx.isValid()) {
        m_selectedId = m_model->robotIdAt(idx.row());
        m_robotManager->setSelectedRobot(m_selectedId);
        auto *robot = m_robotManager->robot(m_selectedId);
        const bool connected = robot && (robot->state() == RobotConnectionState::Connected
            || robot->state() == RobotConnectionState::Connecting);
        const bool canConnect = robot && (robot->state() == RobotConnectionState::Disconnected
            || robot->state() == RobotConnectionState::Error);
        m_connectBtn->setEnabled(canConnect);
        m_disconnectBtn->setEnabled(connected);
        emit currentRobotChanged(m_selectedId);
    } else {
        m_selectedId.clear();
        m_connectBtn->setEnabled(false);
        m_disconnectBtn->setEnabled(false);
    }
    refreshButtons();
}

void RobotListWidget::refreshButtons() {
    bool canConnectAny = false;
    bool connectedAny = false;
    for (auto *robot : m_robotManager->allRobots()) {
        canConnectAny = canConnectAny || robot->state() == RobotConnectionState::Disconnected
            || robot->state() == RobotConnectionState::Error;
        connectedAny = connectedAny || robot->state() == RobotConnectionState::Connected
            || robot->state() == RobotConnectionState::Connecting;
    }
    m_connectAllBtn->setEnabled(canConnectAny);
    m_disconnectAllBtn->setEnabled(connectedAny);
}
