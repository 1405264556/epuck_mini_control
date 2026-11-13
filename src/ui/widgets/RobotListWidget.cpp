#include "RobotListWidget.h"
#include "../models/RobotListModel.h"
#include "../delegates/RobotItemDelegate.h"
#include "ui/Icons.h"
#include "core/RobotManager.h"
#include "core/RobotInstance.h"
#include "comm/SerialManager.h"
#include "comm/BLEDeviceInfo.h"
#include <QEvent>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace {
bool canConnect(const RobotInstance *robot) {
    return robot && (robot->state() == RobotConnectionState::Disconnected
                     || robot->state() == RobotConnectionState::Error);
}
// Dynamic properties used by the style sheet only take effect after a re-polish.
void repolish(QWidget *widget) {
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}
bool isLinked(const RobotInstance *robot) {
    return robot && (robot->state() == RobotConnectionState::Connected
                     || robot->state() == RobotConnectionState::Connecting);
}
}

RobotListWidget::RobotListWidget(RobotManager *robotMgr, SerialManager *serialMgr,
                                   QWidget *parent)
    : QWidget(parent), m_robotManager(robotMgr), m_serialManager(serialMgr)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 8);
    layout->setSpacing(6);

    m_scanBtn = new QPushButton("扫描设备");
    m_scanBtn->setObjectName("scanDevices");
    m_scanBtn->setProperty("accent", true);
    m_scanBtn->setIcon(icons::make(icons::Glyph::Scan, Qt::white));
    m_scanBtn->setToolTip("在后台检测已配对的蓝牙 / USB 串口（Ctrl+R）");
    m_connectBtn = new QPushButton("连接");
    m_connectBtn->setObjectName("toggleConnection");
    m_connectAllBtn = new QPushButton("连接全部");
    m_connectAllBtn->setIcon(icons::make(icons::Glyph::LinkAll));
    m_disconnectAllBtn = new QPushButton("全部断开");
    m_disconnectAllBtn->setIcon(icons::make(icons::Glyph::Unlink));
    m_scanProgress = new QProgressBar;
    m_scanProgress->setTextVisible(true);
    m_scanProgress->setVisible(false);

    layout->addWidget(m_scanBtn);
    layout->addWidget(m_scanProgress);

    m_model = new RobotListModel(m_robotManager, this);
    m_delegate = new RobotItemDelegate(this);
    m_listView = new QListView;
    m_listView->setObjectName("robotList");
    m_listView->setModel(m_model);
    m_listView->setItemDelegate(m_delegate);
    m_listView->setMouseTracking(true);
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    layout->addWidget(m_listView, 1);

    // Guidance drawn over the empty list; clicks pass through to the view.
    m_emptyHint = new QLabel(m_listView->viewport());
    m_emptyHint->setObjectName("hintLabel");
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setWordWrap(true);
    m_emptyHint->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_listView->viewport()->installEventFilter(this);

    layout->addWidget(m_connectBtn);
    auto *allButtons = new QHBoxLayout;
    allButtons->setSpacing(6);
    allButtons->addWidget(m_connectAllBtn);
    allButtons->addWidget(m_disconnectAllBtn);
    layout->addLayout(allButtons);

    connect(m_scanBtn, &QPushButton::clicked, this, &RobotListWidget::onScanClicked);
    connect(m_connectBtn, &QPushButton::clicked, this, [this]() { toggleConnection(m_selectedId); });
    connect(m_listView, &QListView::doubleClicked, this, [this](const QModelIndex &index) {
        toggleConnection(m_model->robotIdAt(index.row()));
    });
    connect(m_listView, &QListView::customContextMenuRequested,
            this, &RobotListWidget::showContextMenu);
    connect(m_connectAllBtn, &QPushButton::clicked, this, [this]() {
        QList<RobotId> ids;
        for (auto *robot : m_robotManager->allRobots()) {
            if (canConnect(robot)) ids.append(robot->id());
        }
        if (!ids.isEmpty()) emit connectAllRequested(ids);
    });
    connect(m_disconnectAllBtn, &QPushButton::clicked, this, [this]() {
        QList<RobotId> ids;
        for (auto *robot : m_robotManager->allRobots()) {
            if (isLinked(robot)) ids.append(robot->id());
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
                    m_robotManager->setSelectedRobot(robot->id());
                    selectRow(robot->id());
                }
                refreshButtons();
            });
    connect(m_serialManager, &SerialManager::robotReconnecting, this,
            [this](const RobotId &id, int attempt, int maxAttempts, int) {
        m_model->setNote(id, QString("重连中 %1/%2…").arg(attempt).arg(maxAttempts));
    });
    connect(m_robotManager, &RobotManager::robotConnectionStateChanged, this,
            [this](const RobotId &, RobotConnectionState) { refreshButtons(); });
    connect(m_robotManager, &RobotManager::robotAdded, this, [this](const RobotId &) { refreshButtons(); });
    connect(m_robotManager, &RobotManager::robotRemoved, this, [this](const RobotId &id) {
        if (id == m_selectedId) m_selectedId.clear();
        refreshButtons();
    });
    connect(m_serialManager, &SerialManager::scanStarted, this, [this]() {
        m_scanBtn->setText("停止扫描");
        m_scanBtn->setProperty("accent", false);
        m_scanBtn->setIcon(icons::make(icons::Glyph::Stop));
        repolish(m_scanBtn);
        m_scanProgress->setRange(0, 0);
        m_scanProgress->setFormat("正在枚举端口…");
        m_scanProgress->setVisible(true);
        refreshEmptyHint();
    });
    connect(m_serialManager, &SerialManager::scanProgress, this,
            [this](int completed, int total, const QString &portName, bool verified) {
        m_scanProgress->setRange(0, qMax(1, total));
        m_scanProgress->setValue(completed);
        m_scanProgress->setFormat(QString("%1/%2  %3 %4")
            .arg(completed).arg(total).arg(portName, verified ? "已识别" : "无响应"));
    });
    connect(m_serialManager, &SerialManager::scanFinished, this, [this]() {
        m_scannedOnce = true;
        m_scanBtn->setText("扫描设备");
        m_scanBtn->setProperty("accent", true);
        m_scanBtn->setIcon(icons::make(icons::Glyph::Scan, Qt::white));
        repolish(m_scanBtn);
        m_scanProgress->setVisible(false);
        refreshButtons();
    });
    connect(m_robotManager, &RobotManager::selectedRobotChanged, this, &RobotListWidget::selectRow);
    if (m_model->rowCount({}) > 0) {
        const RobotId initial = m_robotManager->selectedRobotId();
        if (initial.isEmpty()) m_listView->setCurrentIndex(m_model->index(0, 0));
        else selectRow(initial);
    }
    refreshButtons();
}

bool RobotListWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_listView->viewport() && event->type() == QEvent::Resize) {
        m_emptyHint->setGeometry(m_listView->viewport()->rect().adjusted(12, 12, -12, -12));
    }
    return QWidget::eventFilter(watched, event);
}

void RobotListWidget::selectRow(const RobotId &id) {
    const int row = m_model->rowOf(id);
    if (row >= 0 && m_listView->currentIndex().row() != row) {
        m_listView->setCurrentIndex(m_model->index(row, 0));
    }
}

void RobotListWidget::onScanClicked() {
    if (m_serialManager->isScanning()) { m_serialManager->stopScan(); return; }
    m_serialManager->scanPorts();
}

void RobotListWidget::toggleConnection(const RobotId &id) {
    auto *robot = m_robotManager->robot(id);
    if (!robot) return;
    if (canConnect(robot)) {
        emit connectRequested(id);
    } else if (isLinked(robot)) {
        m_serialManager->disconnectRobot(id);
        emit disconnectRequested(id);
    }
}

void RobotListWidget::showContextMenu(const QPoint &pos) {
    const QModelIndex index = m_listView->indexAt(pos);
    QMenu menu(this);
    if (index.isValid()) {
        const RobotId id = m_model->robotIdAt(index.row());
        auto *robot = m_robotManager->robot(id);
        if (canConnect(robot)) {
            menu.addAction(icons::make(icons::Glyph::Link), "连接",
                           this, [this, id]() { toggleConnection(id); });
            menu.addAction(icons::make(icons::Glyph::Trash), "从列表移除",
                           this, [this, id]() { emit removeRequested(id); });
        } else if (isLinked(robot)) {
            menu.addAction(icons::make(icons::Glyph::Unlink), "断开",
                           this, [this, id]() { toggleConnection(id); });
        }
        menu.addSeparator();
    }
    menu.addAction(icons::make(icons::Glyph::Scan),
                   m_serialManager->isScanning() ? "停止扫描" : "扫描设备",
                   this, &RobotListWidget::onScanClicked);
    menu.addAction("手动输入端口…", this, [this]() { emit manualConnectRequested(); });
    menu.exec(m_listView->viewport()->mapToGlobal(pos));
}

void RobotListWidget::onSelectionChanged() {
    const auto index = m_listView->currentIndex();
    if (index.isValid()) {
        m_selectedId = m_model->robotIdAt(index.row());
        m_robotManager->setSelectedRobot(m_selectedId);
        emit currentRobotChanged(m_selectedId);
    } else {
        m_selectedId.clear();
    }
    refreshButtons();
}

void RobotListWidget::refreshButtons() {
    bool canConnectAny = false;
    bool linkedAny = false;
    for (auto *robot : m_robotManager->allRobots()) {
        canConnectAny = canConnectAny || canConnect(robot);
        linkedAny = linkedAny || isLinked(robot);
    }
    m_connectAllBtn->setEnabled(canConnectAny);
    m_disconnectAllBtn->setEnabled(linkedAny);

    auto *selected = m_robotManager->robot(m_selectedId);
    const bool linked = isLinked(selected);
    m_connectBtn->setEnabled(canConnect(selected) || linked);
    m_connectBtn->setText(!selected ? "连接"
        : linked ? QString("断开 %1").arg(selected->deviceInfo().portName)
                 : QString("连接 %1").arg(selected->deviceInfo().portName));
    m_connectBtn->setIcon(icons::make(linked ? icons::Glyph::Unlink : icons::Glyph::Link));
    refreshEmptyHint();
}

void RobotListWidget::refreshEmptyHint() {
    const bool empty = m_model->rowCount({}) == 0;
    m_emptyHint->setVisible(empty);
    if (!empty) return;
    m_emptyHint->setGeometry(m_listView->viewport()->rect().adjusted(12, 12, -12, -12));
    if (m_serialManager->isScanning()) {
        m_emptyHint->setText("正在检测端口…");
    } else if (m_scannedOnce) {
        m_emptyHint->setText("没有发现机器人\n\n请确认机器人已开机并在系统中完成蓝牙配对，"
                             "且端口没有被其他程序占用。\n\n也可以右键选择「手动输入端口」。");
    } else {
        m_emptyHint->setText("还没有设备\n\n机器人开机并完成蓝牙配对后，\n点击上方「扫描设备」。");
    }
}
