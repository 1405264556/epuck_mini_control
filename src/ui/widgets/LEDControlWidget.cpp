#include "LEDControlWidget.h"
#include "core/RobotManager.h"
#include "core/RobotInstance.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>

LEDControlWidget::LEDControlWidget(RobotManager *robotMgr, QWidget *parent)
    : QWidget(parent), m_robotManager(robotMgr)
{
    auto *layout = new QVBoxLayout(this);

    // ---- Ring LEDs (arranged in a circle-like grid) ----
    layout->addWidget(new QLabel("环形 LED (0-7)"));
    auto *ringGrid = new QGridLayout;
    ringGrid->setSpacing(4);

    // Arrange 8 LEDs in a circular pattern using a 3x3 grid
    // Positions:  top row: LED2(center),  middle row: LED3(left) LED7(right),  bottom row: LED4(left) LED5 LED6(right)
    //   LED0=front-right, LED1=FR-diag, LED2=front, LED3=FL-diag,
    //   LED4=front-left, LED5=left, LED6=rear, LED7=right
    struct { int row, col; } pos[8] = {
        {0, 2}, // LED0: front-right
        {0, 1}, // LED1: FR-diag
        {0, 0}, // LED2: front (top)
        {1, 0}, // LED3: FL-diag
        {1, 1}, // LED4: front-left
        {2, 0}, // LED5: left
        {2, 1}, // LED6: rear
        {2, 2}, // LED7: right
    };

    for (int i = 0; i < 8; ++i) {
        m_ringLEDs[i] = new QPushButton(QString::number(i));
        m_ringLEDs[i]->setFixedSize(40, 40);
        m_ringLEDs[i]->setCheckable(true);
        m_ringLEDs[i]->setStyleSheet(
            "QPushButton { background-color: #f2f5f6; color: #405660; border: 2px solid #c5d1d6; border-radius: 20px; font-weight: bold; }"
            "QPushButton:checked { background-color: #b94558; color: white; border-color: #b94558; }");
        connect(m_ringLEDs[i], &QPushButton::clicked, this, &LEDControlWidget::onRingLEDClicked);
        ringGrid->addWidget(m_ringLEDs[i], pos[i].row, pos[i].col, Qt::AlignCenter);
    }
    layout->addLayout(ringGrid);

    // Separator
    auto *sep1 = new QFrame; sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet("QFrame { color: #c5d1d6; }");
    layout->addWidget(sep1);

    // ---- Body LED ----
    layout->addWidget(new QLabel("机身 LED (底部绿色)"));
    m_bodyLED = new QPushButton("机身灯 - 关");
    m_bodyLED->setCheckable(true);
    m_bodyLED->setStyleSheet(
        "QPushButton { background-color: #f2f5f6; color: #087f72; border: 1px solid #c5d1d6; padding: 8px; }"
        "QPushButton:checked { background-color: #087f72; color: white; border-color: #087f72; }");
    connect(m_bodyLED, &QPushButton::clicked, this, &LEDControlWidget::onBodyLEDClicked);
    layout->addWidget(m_bodyLED);

    // ---- Front LED ----
    layout->addWidget(new QLabel("前灯 (红色)"));
    m_frontLED = new QPushButton("前灯 - 关");
    m_frontLED->setCheckable(true);
    m_frontLED->setStyleSheet(
        "QPushButton { background-color: #f2f5f6; color: #b94558; border: 1px solid #c5d1d6; padding: 8px; }"
        "QPushButton:checked { background-color: #b94558; color: white; border-color: #b94558; }");
    connect(m_frontLED, &QPushButton::clicked, this, &LEDControlWidget::onFrontLEDClicked);
    layout->addWidget(m_frontLED);

    // ---- All Off button ----
    auto *sep2 = new QFrame; sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet("QFrame { color: #c5d1d6; }");
    layout->addWidget(sep2);

    auto *allOffBtn = new QPushButton("全部关闭");
    connect(allOffBtn, &QPushButton::clicked, this, &LEDControlWidget::onAllOff);
    layout->addWidget(allOffBtn);

    layout->addStretch();
    connect(m_robotManager, &RobotManager::robotConnectionStateChanged, this,
        [this](const RobotId &id, RobotConnectionState state) {
            if (id == m_currentId) setEnabled(state == RobotConnectionState::Connected);
        });
    setEnabled(false);
}

void LEDControlWidget::setCurrentRobot(const RobotId &id) {
    if (!m_currentId.isEmpty()) {
        auto &old = m_requestedStates[m_currentId];
        for (int i = 0; i < 8; ++i) old[i] = m_ringState[i];
        old[8] = m_bodyOn;
        old[9] = m_frontOn;
    }
    m_currentId = id;
    const auto state = m_requestedStates.value(id);
    for (int i = 0; i < 8; ++i) { m_ringState[i] = state[i]; m_ringLEDs[i]->setChecked(state[i]); }
    m_bodyOn = state[8]; m_bodyLED->setChecked(m_bodyOn);
    m_frontOn = state[9]; m_frontLED->setChecked(m_frontOn);
    m_bodyLED->setText(m_bodyOn ? "机身灯 - 开" : "机身灯 - 关");
    m_frontLED->setText(m_frontOn ? "前灯 - 开" : "前灯 - 关");
    auto *robot = m_robotManager->robot(id);
    setEnabled(robot && robot->state() == RobotConnectionState::Connected);
}

void LEDControlWidget::onRingLEDClicked() {
    auto *btn = qobject_cast<QPushButton *>(sender());
    if (!btn) return;
    int idx = btn->text().toInt();
    m_ringState[idx] = btn->isChecked();

    if (auto *r = m_robotManager->robot(m_currentId)) {
        r->setLED(idx, m_ringState[idx] ? 1 : 0);
    }
}

void LEDControlWidget::onBodyLEDClicked() {
    m_bodyOn = m_bodyLED->isChecked();
    m_bodyLED->setText(m_bodyOn ? "机身灯 - 开" : "机身灯 - 关");
    if (auto *r = m_robotManager->robot(m_currentId)) {
        r->setBodyLED(m_bodyOn ? 1 : 0);
    }
}

void LEDControlWidget::onFrontLEDClicked() {
    m_frontOn = m_frontLED->isChecked();
    m_frontLED->setText(m_frontOn ? "前灯 - 开" : "前灯 - 关");
    if (auto *r = m_robotManager->robot(m_currentId)) {
        r->setFrontLED(m_frontOn);
    }
}

void LEDControlWidget::onAllOff() {
    for (int i = 0; i < 8; ++i) {
        m_ringLEDs[i]->setChecked(false);
        m_ringState[i] = false;
    }
    m_bodyLED->setChecked(false);
    m_bodyOn = false;
    m_bodyLED->setText("机身灯 - 关");
    m_frontLED->setChecked(false);
    m_frontOn = false;
    m_frontLED->setText("前灯 - 关");

    if (auto *r = m_robotManager->robot(m_currentId)) {
        for (int i = 0; i < 8; ++i) r->setLED(i, 0);
        r->setBodyLED(0);
        r->setFrontLED(false);
    }
}

void LEDControlWidget::updateButtonStyle(QPushButton *btn, bool on) {
    // Used by the old RGB code; not needed for toggle mode
    Q_UNUSED(btn); Q_UNUSED(on);
}
