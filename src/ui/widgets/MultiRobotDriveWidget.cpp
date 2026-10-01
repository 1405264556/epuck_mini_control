#include "MultiRobotDriveWidget.h"
#include "core/RobotInstance.h"
#include "core/RobotManager.h"
#include "control/MultiRobotCoordinator.h"
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QTableWidget>
#include <QTimer>
#include <QDateTime>
#include <QHideEvent>
#include <QSignalBlocker>

MultiRobotDriveWidget::MultiRobotDriveWidget(RobotManager *manager, MultiRobotCoordinator *coordinator, QWidget *parent)
    : QWidget(parent), m_robotManager(manager), m_coordinator(coordinator)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    auto *group = new QGroupBox("同步控制");
    group->setMaximumWidth(310);
    auto *controls = new QGridLayout(group);
    controls->setContentsMargins(8, 8, 8, 8);
    controls->setVerticalSpacing(4);
    controls->addWidget(createValueControl("速度", &m_linearSlider, &m_linearSpin, -1000, 1000, 300), 0, 0, 1, 5);
    controls->addWidget(createValueControl("转向", &m_turnSlider, &m_turnSpin, -1000, 1000, 0), 1, 0, 1, 5);
    const QList<QStyle::StandardPixmap> icons{QStyle::SP_ArrowLeft, QStyle::SP_ArrowUp,
        QStyle::SP_ArrowDown, QStyle::SP_ArrowRight, QStyle::SP_MediaStop};
    const QStringList tips{"左转", "前进", "后退", "右转", "停止同步组"};
    for (int i = 0; i < 5; ++i) {
        auto *button = new QPushButton;
        button->setIcon(style()->standardIcon(icons[i]));
        button->setToolTip(tips[i]);
        button->setAccessibleName(tips[i]);
        button->setMinimumSize(36, 32);
        controls->addWidget(button, 2, i);
        if (i == 4) {
            connect(button, &QPushButton::clicked, this, &MultiRobotDriveWidget::stopSelectedRobots);
        } else {
            connect(button, &QPushButton::pressed, this, [this, i]() {
                m_heldTargets = selectedRobotIds();
                const int speed = qAbs(m_linearSpin->value());
                const int turn = qMax(100, qAbs(m_turnSpin->value()));
                sendGroupMotion(i == 1 ? speed : i == 2 ? -speed : 0, i == 0 ? -turn : i == 3 ? turn : 0);
            });
            connect(button, &QPushButton::released, this, [this]() {
                QMap<RobotId, WheelSpeeds> stops;
                for (const auto &id : m_heldTargets) { stops[id] = {}; m_activeCommands.remove(id); }
                m_heldTargets.clear();
                dispatch(stops);
            });
        }
    }
    auto *apply = new QPushButton("应用到同步组");
    apply->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    apply->setObjectName("applyFleetCommand");
    controls->addWidget(apply, 3, 0, 1, 5);
    connect(apply, &QPushButton::clicked, this, &MultiRobotDriveWidget::sendGroupValues);
    auto *all = new QCheckBox("选择全部在线设备");
    all->setChecked(true);
    controls->addWidget(all, 4, 0, 1, 5);
    m_groupStatus = new QLabel;
    m_groupStatus->setMinimumHeight(18);
    controls->addWidget(m_groupStatus, 5, 0, 1, 5);
    controls->setRowStretch(6, 1);
    layout->addWidget(group);
    m_robotTable = new QTableWidget(0, 8);
    m_robotTable->setObjectName("fleetDriveTable");
    m_robotTable->setHorizontalHeaderLabels({"同步", "设备", "数据时效", "左轮", "右轮", "已下发", "", ""});
    m_robotTable->verticalHeader()->setDefaultSectionSize(36);
    m_robotTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_robotTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_robotTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_robotTable->setAlternatingRowColors(true);
    m_robotTable->horizontalHeader()->setMinimumSectionSize(34);
    for (int i = 0; i < 8; ++i) m_robotTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    m_robotTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int column : {3, 4, 5, 6, 7}) {
        m_robotTable->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Fixed);
        m_robotTable->setColumnWidth(column, column <= 4 ? 100 : column == 5 ? 110 : 42);
    }
    layout->addWidget(m_robotTable, 1);
    connect(all, &QCheckBox::toggled, this, [this](bool checked) {
        for (int row = 0; row < m_robotTable->rowCount(); ++row)
            m_robotTable->cellWidget(row, 0)->findChild<QCheckBox *>()->setChecked(checked);
    });
    connect(m_robotTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        m_robotManager->setSelectedRobot(m_robotTable->item(row, 1)->data(Qt::UserRole).toString());
    });
    connect(manager, &RobotManager::robotAdded, this, [this](const RobotId &) { rebuildRobotRows(); });
    connect(manager, &RobotManager::robotRemoved, this, [this](const RobotId &id) {
        m_activeCommands.remove(id); rebuildRobotRows();
    });
    connect(manager, &RobotManager::robotConnectionStateChanged, this, [this](const RobotId &id, RobotConnectionState state) {
        if (state != RobotConnectionState::Connected) m_activeCommands.remove(id);
        refreshRows();
    });
    if (coordinator) connect(coordinator, &MultiRobotCoordinator::coordinationStarted, this, [this]() {
        m_activeCommands.clear(); m_heldTargets.clear();
    });
    m_heartbeat = new QTimer(this);
    m_heartbeat->setInterval(100);
    connect(m_heartbeat, &QTimer::timeout, this, [this]() {
        if (isVisible() && !m_activeCommands.isEmpty()) dispatch(m_activeCommands);
        refreshRows();
    });
    m_heartbeat->start();
    rebuildRobotRows();
}

QWidget *MultiRobotDriveWidget::createValueControl(const QString &label, QSlider **slider,
                                                  QSpinBox **spin, int minimum, int maximum, int value) {
    auto *widget = new QWidget;
    auto *layout = new QGridLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    *slider = new QSlider(Qt::Horizontal);
    (*slider)->setRange(minimum, maximum);
    (*slider)->setValue(value);
    *spin = new QSpinBox;
    (*spin)->setRange(minimum, maximum);
    (*spin)->setSingleStep(25);
    (*spin)->setAccelerated(true);
    (*spin)->setKeyboardTracking(false);
    (*spin)->setValue(value);
    (*spin)->setToolTip("步 / 秒，范围 -1000 到 1000");
    (*spin)->setFixedWidth(82);
    layout->addWidget(new QLabel(label), 0, 0);
    layout->addWidget(*slider, 0, 1);
    layout->addWidget(*spin, 0, 2);
    layout->setColumnStretch(1, 1);
    connect(*slider, &QSlider::valueChanged, *spin, &QSpinBox::setValue);
    connect(*spin, QOverload<int>::of(&QSpinBox::valueChanged), *slider, &QSlider::setValue);
    return widget;
}

void MultiRobotDriveWidget::rebuildRobotRows() {
    QMap<RobotId, QList<int>> saved;
    for (int row = 0; row < m_robotTable->rowCount(); ++row) {
        const auto id = m_robotTable->item(row, 1)->data(Qt::UserRole).toString();
        saved[id] = {m_robotTable->cellWidget(row, 0)->findChild<QCheckBox *>()->isChecked() ? 1 : 0,
            qobject_cast<QSpinBox *>(m_robotTable->cellWidget(row, 3))->value(),
            qobject_cast<QSpinBox *>(m_robotTable->cellWidget(row, 4))->value()};
    }
    m_robotTable->setRowCount(0);
    for (auto *robot : m_robotManager->allRobots()) {
        const RobotId id = robot->id();
        const int row = m_robotTable->rowCount();
        m_robotTable->insertRow(row);
        auto *check = new QCheckBox;
        check->setChecked(!saved.contains(id) || saved[id][0]);
        check->setProperty("robotId", id);
        auto *container = new QWidget;
        auto *box = new QHBoxLayout(container);
        box->setContentsMargins(0, 0, 0, 0);
        box->setAlignment(Qt::AlignCenter);
        box->addWidget(check);
        m_robotTable->setCellWidget(row, 0, container);
        auto *name = new QTableWidgetItem(robot->deviceInfo().portName.isEmpty() ? id : robot->deviceInfo().portName);
        name->setData(Qt::UserRole, id);
        name->setToolTip(robot->name());
        m_robotTable->setItem(row, 1, name);
        m_robotTable->setItem(row, 2, new QTableWidgetItem);
        m_robotTable->setItem(row, 5, new QTableWidgetItem);
        for (int col : {3, 4}) {
            auto *spin = new QSpinBox;
            spin->setRange(-1000, 1000);
            spin->setSingleStep(25);
            spin->setKeyboardTracking(false);
            spin->setAccelerated(true);
            spin->setMinimumWidth(76);
            spin->setValue(saved.value(id).value(col - 2));
            spin->setToolTip("步 / 秒");
            m_robotTable->setCellWidget(row, col, spin);
        }
        for (int col : {6, 7}) {
            auto *button = new QPushButton;
            button->setIcon(style()->standardIcon(col == 6 ? QStyle::SP_DialogApplyButton : QStyle::SP_MediaStop));
            button->setToolTip(col == 6 ? "应用该设备轮速" : "停止该设备");
            button->setAccessibleName(button->toolTip());
            m_robotTable->setCellWidget(row, col, button);
            connect(button, &QPushButton::clicked, this, [this, id, row, col]() {
                auto *left = qobject_cast<QSpinBox *>(m_robotTable->cellWidget(row, 3));
                auto *right = qobject_cast<QSpinBox *>(m_robotTable->cellWidget(row, 4));
                if (col == 7) { left->setValue(0); right->setValue(0); }
                m_robotManager->setSelectedRobot(id);
                sendWheelValues(id, left->value(), right->value());
            });
        }
        connect(check, &QCheckBox::toggled, this, [this, id](bool checked) {
            if (!checked && m_activeCommands.contains(id)) {
                m_activeCommands.remove(id);
                dispatch({{id, {}}});
            }
            refreshRows();
        });
    }
    refreshRows();
}

void MultiRobotDriveWidget::refreshRows() {
    const auto now = QDateTime::currentMSecsSinceEpoch();
    for (int row = 0; row < m_robotTable->rowCount(); ++row) {
        auto *r = m_robotManager->robot(m_robotTable->item(row, 1)->data(Qt::UserRole).toString());
        if (!r) continue;
        const bool online = r->state() == RobotConnectionState::Connected;
        m_robotTable->cellWidget(row, 0)->findChild<QCheckBox *>()->setEnabled(online);
        for (int col : {3, 4, 6, 7}) m_robotTable->cellWidget(row, col)->setEnabled(online);
        const auto &data = r->latestSensorData();
        m_robotTable->item(row, 2)->setText(online && data.timestamp
            ? QString::number(qMax<qint64>(0, now-data.timestamp)) + " ms" : stateText(r->state()));
        m_robotTable->item(row, 2)->setForeground(online && data.isFresh(SensorData::Proximity, now)
            ? QColor("#087f72") : QColor("#a85d22"));
        const auto speeds = r->commandedSpeeds();
        m_robotTable->item(row, 5)->setText(QString("%1 / %2").arg(qRound(speeds.left*1000)).arg(qRound(speeds.right*1000)));
    }
    m_groupStatus->setText(QString("同步组 %1 台 · 在线 %2 台").arg(selectedRobotIds().size()).arg(m_robotManager->connectedRobots().size()));
}

QList<RobotId> MultiRobotDriveWidget::selectedRobotIds() const {
    QList<RobotId> ids;
    for (int row = 0; row < m_robotTable->rowCount(); ++row) {
        auto *check = m_robotTable->cellWidget(row, 0)->findChild<QCheckBox *>();
        if (check->isChecked() && check->isEnabled()) ids.append(check->property("robotId").toString());
    }
    return ids;
}

void MultiRobotDriveWidget::dispatch(const QMap<RobotId, WheelSpeeds> &commands) {
    if (m_coordinator && !commands.isEmpty()) m_coordinator->manualDrive(commands);
}

void MultiRobotDriveWidget::sendGroupValues() { sendGroupMotion(m_linearSpin->value(), m_turnSpin->value()); }
void MultiRobotDriveWidget::sendGroupMotion(int linear, int turn) {
    QMap<RobotId, WheelSpeeds> commands;
    for (const auto &id : selectedRobotIds()) {
        const WheelSpeeds speeds{qBound(-1.0, (linear + turn) / 1000.0, 1.0),
                                 qBound(-1.0, (linear - turn) / 1000.0, 1.0)};
        commands[id] = speeds;
        m_activeCommands[id] = speeds;
    }
    dispatch(commands);
}
void MultiRobotDriveWidget::stopSelectedRobots() {
    QMap<RobotId, WheelSpeeds> commands;
    for (const auto &id : selectedRobotIds()) { commands[id] = {}; m_activeCommands.remove(id); }
    dispatch(commands);
}
void MultiRobotDriveWidget::stopManualControl() {
    QMap<RobotId, WheelSpeeds> commands;
    for (auto it = m_activeCommands.cbegin(); it != m_activeCommands.cend(); ++it) commands[it.key()] = {};
    m_activeCommands.clear();
    m_heldTargets.clear();
    dispatch(commands);
}
void MultiRobotDriveWidget::hideEvent(QHideEvent *event) { stopManualControl(); QWidget::hideEvent(event); }
void MultiRobotDriveWidget::sendWheelValues(const RobotId &id, int left, int right) {
    auto *robot = m_robotManager->robot(id);
    if (!robot || robot->state() != RobotConnectionState::Connected) return;
    const WheelSpeeds speeds{left/1000.0, right/1000.0};
    if (left || right) m_activeCommands[id] = speeds; else m_activeCommands.remove(id);
    dispatch({{id, speeds}});
}
QString MultiRobotDriveWidget::stateText(RobotConnectionState state) const {
    switch (state) {
    case RobotConnectionState::Connecting: return "连接中";
    case RobotConnectionState::Connected: return "等待数据";
    case RobotConnectionState::Disconnecting: return "断开中";
    case RobotConnectionState::Error: return "异常";
    case RobotConnectionState::Disconnected: return "离线";
    }
    return "--";
}
