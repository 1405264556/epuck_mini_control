#include "MultiRobotDriveWidget.h"

#include "core/RobotInstance.h"
#include "core/RobotManager.h"

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
#include <QVBoxLayout>

MultiRobotDriveWidget::MultiRobotDriveWidget(RobotManager *robotManager, QWidget *parent)
    : QWidget(parent), m_robotManager(robotManager)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *groupBox = new QGroupBox("多机器人同步控制");
    auto *groupLayout = new QGridLayout(groupBox);
    groupLayout->addWidget(createValueControl("前进速度", &m_linearSlider, &m_linearSpin,
                                               -1000, 1000, 500), 0, 0, 1, 5);
    groupLayout->addWidget(createValueControl("转向量", &m_turnSlider, &m_turnSpin,
                                               -1000, 1000, 350), 1, 0, 1, 5);

    auto *forwardButton = new QPushButton;
    auto *backButton = new QPushButton;
    auto *leftButton = new QPushButton;
    auto *rightButton = new QPushButton;
    auto *sendButton = new QPushButton("应用数值");
    auto *stopButton = new QPushButton;

    forwardButton->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    backButton->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    leftButton->setIcon(style()->standardIcon(QStyle::SP_ArrowLeft));
    rightButton->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
    stopButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    forwardButton->setToolTip("按住：选中机器人前进");
    backButton->setToolTip("按住：选中机器人后退");
    leftButton->setToolTip("按住：选中机器人原地左转");
    rightButton->setToolTip("按住：选中机器人原地右转");
    stopButton->setToolTip("停止所有勾选的机器人");

    groupLayout->addWidget(leftButton, 2, 0);
    groupLayout->addWidget(forwardButton, 2, 1);
    groupLayout->addWidget(backButton, 2, 2);
    groupLayout->addWidget(rightButton, 2, 3);
    groupLayout->addWidget(stopButton, 2, 4);
    groupLayout->addWidget(sendButton, 3, 0, 1, 5);
    layout->addWidget(groupBox);

    auto *individualBox = new QGroupBox("单机器人分控");
    auto *individualLayout = new QVBoxLayout(individualBox);
    m_robotTable = new QTableWidget(0, 7);
    m_robotTable->setHorizontalHeaderLabels(
        {"同步", "机器人", "状态", "左轮", "右轮", "应用", "停止"});
    m_robotTable->verticalHeader()->setVisible(false);
    m_robotTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_robotTable->setAlternatingRowColors(true);
    m_robotTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_robotTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_robotTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_robotTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_robotTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_robotTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_robotTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    individualLayout->addWidget(m_robotTable);
    layout->addWidget(individualBox, 1);

    auto pressMotion = [this](QPushButton *button, int linear, int turn) {
        connect(button, &QPushButton::pressed, this, [this, linear, turn]() {
            const int speed = qMax(100, qAbs(m_linearSpin->value()));
            const int steering = qMax(100, qAbs(m_turnSpin->value()));
            sendGroupMotion(linear * speed, turn * steering);
        });
        connect(button, &QPushButton::released, this, &MultiRobotDriveWidget::stopSelectedRobots);
    };
    pressMotion(forwardButton, 1, 0);
    pressMotion(backButton, -1, 0);
    pressMotion(leftButton, 0, -1);
    pressMotion(rightButton, 0, 1);
    connect(sendButton, &QPushButton::clicked, this, &MultiRobotDriveWidget::sendGroupValues);
    connect(stopButton, &QPushButton::clicked, this, &MultiRobotDriveWidget::stopSelectedRobots);

    connect(m_robotManager, &RobotManager::robotAdded,
            this, [this](const RobotId &) { rebuildRobotRows(); });
    connect(m_robotManager, &RobotManager::robotRemoved,
            this, [this](const RobotId &) { rebuildRobotRows(); });
    connect(m_robotManager, &RobotManager::robotConnectionStateChanged,
            this, [this](const RobotId &, RobotConnectionState) { rebuildRobotRows(); });
    rebuildRobotRows();
}

QWidget *MultiRobotDriveWidget::createValueControl(const QString &label, QSlider **slider,
                                                   QSpinBox **spin, int minimum,
                                                   int maximum, int value) {
    auto *widget = new QWidget;
    auto *layout = new QGridLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    *slider = new QSlider(Qt::Horizontal);
    (*slider)->setRange(minimum, maximum);
    (*slider)->setSingleStep(25);
    (*slider)->setPageStep(100);
    (*slider)->setValue(value);
    *spin = new QSpinBox;
    (*spin)->setRange(minimum, maximum);
    (*spin)->setSingleStep(25);
    (*spin)->setAccelerated(true);
    (*spin)->setKeyboardTracking(false);
    (*spin)->setValue(value);
    (*spin)->setSuffix(" step/s");

    layout->addWidget(new QLabel(label), 0, 0);
    layout->addWidget(*slider, 0, 1);
    layout->addWidget(*spin, 0, 2);
    connect(*slider, &QSlider::valueChanged, *spin, &QSpinBox::setValue);
    connect(*spin, QOverload<int>::of(&QSpinBox::valueChanged), *slider, &QSlider::setValue);
    return widget;
}

void MultiRobotDriveWidget::rebuildRobotRows() {
    QMap<RobotId, QList<int>> previousValues;
    for (int row = 0; row < m_robotTable->rowCount(); ++row) {
        auto *nameItem = m_robotTable->item(row, 1);
        auto *checkContainer = m_robotTable->cellWidget(row, 0);
        auto *check = checkContainer ? checkContainer->findChild<QCheckBox *>() : nullptr;
        auto *left = qobject_cast<QSpinBox *>(m_robotTable->cellWidget(row, 3));
        auto *right = qobject_cast<QSpinBox *>(m_robotTable->cellWidget(row, 4));
        if (nameItem && check && left && right) {
            previousValues[nameItem->data(Qt::UserRole).toString()] = {
                check->isChecked() ? 1 : 0, left->value(), right->value()};
        }
    }
    m_robotTable->setRowCount(0);
    const auto robots = m_robotManager->allRobots();
    for (auto *robot : robots) {
        const int row = m_robotTable->rowCount();
        m_robotTable->insertRow(row);
        const bool connected = robot->state() == RobotConnectionState::Connected;

        auto *selected = new QCheckBox;
        const auto saved = previousValues.value(robot->id());
        selected->setChecked(connected && (saved.isEmpty() || saved.value(0) != 0));
        selected->setEnabled(connected);
        selected->setProperty("robotId", robot->id());
        auto *checkContainer = new QWidget;
        auto *checkLayout = new QVBoxLayout(checkContainer);
        checkLayout->setContentsMargins(0, 0, 0, 0);
        checkLayout->setAlignment(Qt::AlignCenter);
        checkLayout->addWidget(selected);
        m_robotTable->setCellWidget(row, 0, checkContainer);

        auto *nameItem = new QTableWidgetItem(robot->name().isEmpty() ? robot->id() : robot->name());
        nameItem->setData(Qt::UserRole, robot->id());
        m_robotTable->setItem(row, 1, nameItem);
        m_robotTable->setItem(row, 2, new QTableWidgetItem(stateText(robot->state())));

        auto *leftSpin = new QSpinBox;
        auto *rightSpin = new QSpinBox;
        for (auto *spin : {leftSpin, rightSpin}) {
            spin->setRange(-1000, 1000);
            spin->setSingleStep(25);
            spin->setAccelerated(true);
            spin->setKeyboardTracking(false);
            spin->setEnabled(connected);
            spin->setSuffix(" step/s");
        }
        if (saved.size() >= 3) {
            leftSpin->setValue(saved[1]);
            rightSpin->setValue(saved[2]);
        }
        m_robotTable->setCellWidget(row, 3, leftSpin);
        m_robotTable->setCellWidget(row, 4, rightSpin);

        auto *applyButton = new QPushButton;
        applyButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
        applyButton->setToolTip("发送该机器人的左右轮数值");
        applyButton->setEnabled(connected);
        auto *stopButton = new QPushButton;
        stopButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
        stopButton->setToolTip("停止该机器人");
        stopButton->setEnabled(connected);
        m_robotTable->setCellWidget(row, 5, applyButton);
        m_robotTable->setCellWidget(row, 6, stopButton);

        const RobotId id = robot->id();
        connect(applyButton, &QPushButton::clicked, this, [this, id, leftSpin, rightSpin]() {
            sendWheelValues(id, leftSpin->value(), rightSpin->value());
        });
        connect(stopButton, &QPushButton::clicked, this, [this, id, leftSpin, rightSpin]() {
            leftSpin->setValue(0);
            rightSpin->setValue(0);
            sendWheelValues(id, 0, 0);
        });
    }
}

QList<RobotId> MultiRobotDriveWidget::selectedRobotIds() const {
    QList<RobotId> ids;
    for (int row = 0; row < m_robotTable->rowCount(); ++row) {
        auto *container = m_robotTable->cellWidget(row, 0);
        auto *check = container ? container->findChild<QCheckBox *>() : nullptr;
        if (check && check->isChecked() && check->isEnabled()) {
            ids.append(check->property("robotId").toString());
        }
    }
    return ids;
}

void MultiRobotDriveWidget::sendGroupValues() {
    sendGroupMotion(m_linearSpin->value(), m_turnSpin->value());
}

void MultiRobotDriveWidget::sendGroupMotion(int linear, int turn) {
    const int left = qBound(-1000, linear + turn, 1000);
    const int right = qBound(-1000, linear - turn, 1000);
    for (const auto &id : selectedRobotIds()) sendWheelValues(id, left, right);
}

void MultiRobotDriveWidget::stopSelectedRobots() {
    for (const auto &id : selectedRobotIds()) sendWheelValues(id, 0, 0);
}

void MultiRobotDriveWidget::sendWheelValues(const RobotId &id, int left, int right) {
    auto *robot = m_robotManager->robot(id);
    if (!robot || robot->state() != RobotConnectionState::Connected) return;
    robot->setMotorSpeeds(left / 1000.0, right / 1000.0);
}

QString MultiRobotDriveWidget::stateText(RobotConnectionState state) const {
    switch (state) {
    case RobotConnectionState::Connecting: return "连接中";
    case RobotConnectionState::Connected: return "在线";
    case RobotConnectionState::Disconnecting: return "断开中";
    case RobotConnectionState::Error: return "异常";
    case RobotConnectionState::Disconnected: return "离线";
    }
    return "未知";
}
