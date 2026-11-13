#include "MotorControlWidget.h"
#include "ui/Icons.h"
#include "core/RobotManager.h"
#include "core/RobotInstance.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSlider>
#include <QPushButton>
#include <QTabWidget>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QtMath>
#include <QTimer>
#include <QSpinBox>
#include <QSignalBlocker>
#include <QHideEvent>
#include <cmath>

// ==================== JoystickWidget ====================

JoystickWidget::JoystickWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(150, 150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::OpenHandCursor);
    setToolTip("按住拖动驾驶，松开即停");
}

double JoystickWidget::padRadius() const {
    return qBound(50.0, (qMin(width(), height()) - 44) / 2.0, 150.0);
}

QPointF JoystickWidget::padCenter() const { return QPointF(width() / 2.0, height() / 2.0); }

void JoystickWidget::reset() {
    m_dragging = false;
    m_knob = {};
    setCursor(Qt::OpenHandCursor);
    update();
}

void JoystickWidget::showCommand(double forward, double turn) {
    if (m_dragging) return;
    QPointF knob(qBound(-1.0, turn, 1.0), qBound(-1.0, forward, 1.0));
    const double length = std::hypot(knob.x(), knob.y());
    if (length > 1.0) knob /= length;
    if (knob == m_knob) return;
    m_knob = knob;
    update();
}

void JoystickWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool enabled = isEnabled();
    const double radius = padRadius();
    const QPointF center = padCenter();
    const double knobRadius = qMax(13.0, radius * 0.2);
    const double travel = radius - knobRadius;

    // Pad
    p.setPen(QPen(QColor(enabled ? "#c3d0d5" : "#dfe6e9"), 1.5));
    p.setBrush(QColor(enabled ? "#f4f8f8" : "#f6f8f9"));
    p.drawEllipse(center, radius, radius);
    p.setPen(QPen(QColor("#dbe4e7"), 1, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(center, travel * 0.5, travel * 0.5);
    p.drawLine(center - QPointF(travel, 0), center + QPointF(travel, 0));
    p.drawLine(center - QPointF(0, travel), center + QPointF(0, travel));

    // Direction labels
    QFont font = p.font();
    font.setPixelSize(11);
    p.setFont(font);
    p.setPen(QColor(enabled ? "#6f828a" : "#b4c0c5"));
    const QRectF bounds(center.x() - radius, center.y() - radius, radius * 2, radius * 2);
    p.drawText(QRectF(bounds.left(), bounds.top() - 18, bounds.width(), 16), Qt::AlignCenter, "前");
    p.drawText(QRectF(bounds.left(), bounds.bottom() + 2, bounds.width(), 16), Qt::AlignCenter, "后");
    p.drawText(QRectF(bounds.left() - 20, bounds.top(), 18, bounds.height()), Qt::AlignCenter, "左");
    p.drawText(QRectF(bounds.right() + 2, bounds.top(), 18, bounds.height()), Qt::AlignCenter, "右");

    // Deflection line and knob
    const QPointF knob = center + QPointF(m_knob.x() * travel, -m_knob.y() * travel);
    const bool deflected = !m_knob.isNull();
    const QColor accent(enabled ? "#087f72" : "#b9c8ce");
    if (deflected) {
        p.setPen(QPen(QColor(8, 127, 114, 90), 3, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(center, knob);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 22));
    p.drawEllipse(knob + QPointF(0, 1.5), knobRadius, knobRadius);
    p.setBrush(deflected || m_dragging ? accent : QColor("#ffffff"));
    p.setPen(QPen(accent, 2));
    p.drawEllipse(knob, knobRadius, knobRadius);
}

void JoystickWidget::mousePressEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton) return;
    m_dragging = true;
    setCursor(Qt::ClosedHandCursor);
    updateFromPos(e->position());
}

void JoystickWidget::mouseMoveEvent(QMouseEvent *e) {
    if (m_dragging) updateFromPos(e->position());
}

void JoystickWidget::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton || !m_dragging) return;
    reset();
    emit released();
}

void JoystickWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::EnabledChange) {
        if (!isEnabled() && m_dragging) { reset(); emit released(); }
        update();
    }
    QWidget::changeEvent(event);
}

void JoystickWidget::updateFromPos(const QPointF &pos) {
    const double radius = padRadius();
    const double travel = radius - qMax(13.0, radius * 0.2);
    QPointF offset = (pos - padCenter()) / travel;
    offset.setY(-offset.y());
    const double length = std::hypot(offset.x(), offset.y());
    if (length > 1.0) offset /= length;
    m_knob = offset;
    update();

    // Rescale past the dead zone so output still starts from zero.
    const double magnitude = qMin(1.0, length);
    if (magnitude < kDeadzone) { emit moved(0, 0); return; }
    const double scaled = (magnitude - kDeadzone) / (1.0 - kDeadzone);
    emit moved(offset.y() / magnitude * scaled, offset.x() / magnitude * scaled);
}

// ==================== MotorControlWidget ====================

WheelSpeeds MotorControlWidget::mixDrive(double forward, double turn, int limitStepsPerSecond) {
    if (!std::isfinite(forward) || !std::isfinite(turn)) return {};
    forward = qBound(-1.0, forward, 1.0);
    // Turning in place at full wheel speed is hard to control on a 5 cm wheelbase.
    turn = qBound(-1.0, turn, 1.0) * 0.7;
    double left = forward + turn;
    double right = forward - turn;
    // Scale both wheels together so a diagonal keeps its curvature instead of
    // saturating one side.
    const double peak = qMax(1.0, qMax(std::abs(left), std::abs(right)));
    const double limit = qBound(0, limitStepsPerSecond, 1000) / 1000.0;
    return {left / peak * limit, right / peak * limit};
}

MotorControlWidget::MotorControlWidget(RobotManager *robotMgr, QWidget *parent)
    : QWidget(parent), m_robotManager(robotMgr)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_tabWidget = new QTabWidget;
    m_tabWidget->setDocumentMode(true);

    // ---- Tab 1: Joystick ----
    auto *joyTab = new QWidget;
    m_joystickPage = joyTab;
    auto *joyLayout = new QVBoxLayout(joyTab);
    joyLayout->setContentsMargins(10, 10, 10, 6);
    joyLayout->setSpacing(4);
    auto *limitRow = new QHBoxLayout;
    auto *limitLabel = new QLabel("限速");
    limitLabel->setToolTip("摇杆和键盘驾驶的最大轮速");
    m_limitSlider = new QSlider(Qt::Horizontal);
    m_limitSlider->setObjectName("speedLimitSlider");
    m_limitSlider->setRange(100, 1000);
    m_limitSlider->setSingleStep(50);
    m_limitSlider->setPageStep(100);
    m_limitSlider->setFocusPolicy(Qt::ClickFocus);
    m_limitSpin = new QSpinBox;
    m_limitSpin->setObjectName("speedLimitSpin");
    m_limitSpin->setRange(100, 1000);
    m_limitSpin->setSingleStep(50);
    m_limitSpin->setSuffix(" 步/秒");
    m_limitSpin->setKeyboardTracking(false);
    m_limitSpin->setFixedWidth(112);
    limitRow->addWidget(limitLabel);
    limitRow->addWidget(m_limitSlider, 1);
    limitRow->addWidget(m_limitSpin);
    connect(m_limitSlider, &QSlider::valueChanged, m_limitSpin, &QSpinBox::setValue);
    connect(m_limitSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        const QSignalBlocker blocker(m_limitSlider);
        m_limitSlider->setValue(value);
        emit speedLimitChanged(value);
    });
    m_limitSpin->setValue(500);
    m_limitSlider->setValue(500);

    joyLayout->addLayout(limitRow);
    m_joystick = new JoystickWidget;
    joyLayout->addWidget(m_joystick, 1);
    m_hint = new QLabel("键盘 W A S D 驾驶 · 空格停止");
    m_hint->setObjectName("hintLabel");
    m_hint->setAlignment(Qt::AlignCenter);
    joyLayout->addWidget(m_hint);
    m_tabWidget->addTab(joyTab, "驾驶");

    // ---- Tab 2: Wheel sliders ----
    auto *sliderTab = new QWidget;
    m_wheelPage = sliderTab;
    auto *grid = new QGridLayout(sliderTab);
    grid->setContentsMargins(8, 12, 8, 8);
    grid->setHorizontalSpacing(24);
    m_leftSlider = new QSlider(Qt::Vertical);
    m_rightSlider = new QSlider(Qt::Vertical);
    m_leftSpin = new QSpinBox;
    m_rightSpin = new QSpinBox;
    const QStringList names{"左轮", "右轮"};
    QSlider *sliders[2] = {m_leftSlider, m_rightSlider};
    QSpinBox *spins[2] = {m_leftSpin, m_rightSpin};
    for (int i = 0; i < 2; ++i) {
        auto *name = new QLabel(names[i]);
        name->setAlignment(Qt::AlignCenter);
        sliders[i]->setRange(-1000, 1000);
        sliders[i]->setPageStep(100);
        sliders[i]->setMinimumHeight(110);
        sliders[i]->setFocusPolicy(Qt::ClickFocus);
        spins[i]->setRange(-1000, 1000);
        spins[i]->setSingleStep(25);
        spins[i]->setKeyboardTracking(false);
        spins[i]->setFixedWidth(84);
        spins[i]->setToolTip("步 / 秒，范围 -1000 到 1000");
        grid->addWidget(name, 0, i, Qt::AlignHCenter);
        grid->addWidget(sliders[i], 1, i, Qt::AlignHCenter);
        grid->addWidget(spins[i], 2, i, Qt::AlignHCenter);
        connect(sliders[i], &QSlider::valueChanged, spins[i], &QSpinBox::setValue);
        connect(spins[i], QOverload<int>::of(&QSpinBox::valueChanged), sliders[i], &QSlider::setValue);
        connect(sliders[i], &QSlider::valueChanged, this, &MotorControlWidget::onSpeedChanged);
    }
    auto *sliderHint = new QLabel("轮速会一直保持，直到点击停止");
    sliderHint->setObjectName("hintLabel");
    sliderHint->setAlignment(Qt::AlignCenter);
    grid->addWidget(sliderHint, 3, 0, 1, 2);
    grid->setRowStretch(1, 1);
    m_tabWidget->addTab(sliderTab, "轮速");
    layout->addWidget(m_tabWidget, 1);

    // ---- Readout and stop ----
    m_readout = new QLabel;
    m_readout->setObjectName("valueReadout");
    m_readout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_stopBtn = new QPushButton("停止");
    m_stopBtn->setObjectName("emergencyStop");
    m_stopBtn->setIcon(icons::make(icons::Glyph::Stop, Qt::white));
    m_stopBtn->setToolTip("停止当前机器人（空格）");
    m_stopBtn->setMinimumHeight(32);
    m_stopBtn->setFocusPolicy(Qt::NoFocus);
    auto *footer = new QHBoxLayout;
    footer->setContentsMargins(2, 0, 2, 0);
    footer->addWidget(m_readout, 1);
    footer->addWidget(m_stopBtn);
    layout->addLayout(footer);

    connect(m_stopBtn, &QPushButton::clicked, this, &MotorControlWidget::onStop);
    connect(m_joystick, &JoystickWidget::moved, this, [this](double forward, double turn) {
        const auto speeds = mixDrive(forward, turn, speedLimit());
        sendSpeeds(speeds.left, speeds.right);
    });
    connect(m_joystick, &JoystickWidget::released, this, [this]() { sendSpeeds(0, 0); });
    // Leaving a tab must not leave its command running behind the other tab.
    connect(m_tabWidget, &QTabWidget::currentChanged, this, [this]() { releaseControl(); });

    m_heartbeat = new QTimer(this);
    m_heartbeat->setInterval(100);
    connect(m_heartbeat, &QTimer::timeout, this, [this]() {
        if (m_moving && isVisible()) sendSpeeds(m_activeSpeeds.left, m_activeSpeeds.right);
    });
    m_heartbeat->start();
    connect(m_robotManager, &RobotManager::robotConnectionStateChanged, this,
            [this](const RobotId &id, RobotConnectionState) { if (id == m_currentId) refreshEnabled(); });
    refreshEnabled();
    refreshReadout();
}

int MotorControlWidget::speedLimit() const { return m_limitSpin->value(); }

void MotorControlWidget::setSpeedLimit(int stepsPerSecond) { m_limitSpin->setValue(stepsPerSecond); }

void MotorControlWidget::addExtraTab(QWidget *page, const QString &title) {
    m_tabWidget->addTab(page, title);
}

bool MotorControlWidget::isJoystickTabActive() const { return m_tabWidget->currentIndex() == 0; }

void MotorControlWidget::showJoystickTab() { m_tabWidget->setCurrentIndex(0); }

void MotorControlWidget::setCurrentRobot(const RobotId &id) {
    if (id != m_currentId) releaseControl();
    m_currentId = id;
    refreshEnabled();
}

void MotorControlWidget::driveNormalized(double forward, double turn) {
    if (!isVisible() || !m_joystickPage->isEnabled() || m_joystick->isDragging()) return;
    if (forward == 0 && turn == 0) {
        m_joystick->showCommand(0, 0);
        if (m_moving) sendSpeeds(0, 0);
        return;
    }
    if (!isJoystickTabActive()) return;
    m_joystick->showCommand(forward, turn);
    const auto speeds = mixDrive(forward, turn, speedLimit());
    sendSpeeds(speeds.left, speeds.right);
}

void MotorControlWidget::onSpeedChanged() {
    if (m_currentId.isEmpty()) return;
    sendSpeeds(m_leftSlider->value() / 1000.0, m_rightSlider->value() / 1000.0);
}

void MotorControlWidget::onStop() {
    sendSpeeds(0, 0);
    releaseControl(false);
    emit stopClicked();
}

void MotorControlWidget::sendSpeeds(double left, double right) {
    auto *robot = m_robotManager->robot(m_currentId);
    if (!robot || robot->state() != RobotConnectionState::Connected) return;
    m_activeSpeeds = {left, right};
    m_moving = left != 0 || right != 0;
    refreshReadout();
    emit manualCommandRequested(m_currentId, left, right);
}

void MotorControlWidget::releaseControl(bool sendStop) {
    if (sendStop && m_moving) sendSpeeds(0, 0);
    m_moving = false;
    m_activeSpeeds = {};
    const QSignalBlocker left(m_leftSlider), right(m_rightSlider), joystick(m_joystick);
    const QSignalBlocker leftSpin(m_leftSpin), rightSpin(m_rightSpin);
    m_leftSlider->setValue(0); m_rightSlider->setValue(0);
    m_leftSpin->setValue(0); m_rightSpin->setValue(0);
    m_joystick->reset();
    refreshReadout();
}

void MotorControlWidget::refreshReadout() {
    auto signedValue = [](double fraction) {
        const int value = qRound(fraction * 1000);
        return value > 0 ? QString("+%1").arg(value) : QString::number(value);
    };
    m_readout->setText(QString("左 %1 · 右 %2 步/秒")
                           .arg(signedValue(m_activeSpeeds.left), signedValue(m_activeSpeeds.right)));
}

void MotorControlWidget::refreshEnabled() {
    auto *r = m_robotManager->robot(m_currentId);
    const bool online = r && r->state() == RobotConnectionState::Connected;
    if (!online) releaseControl(false);
    // Only the two drive pages follow the link state; pages added by the owner
    // (LED, path planning) manage their own availability.
    m_joystickPage->setEnabled(online);
    m_wheelPage->setEnabled(online);
    m_stopBtn->setEnabled(online);
    m_hint->setText(online ? "键盘 W A S D 驾驶 · 空格停止"
                           : "连接机器人后可用摇杆或键盘驾驶");
}

void MotorControlWidget::hideEvent(QHideEvent *event) { releaseControl(); QWidget::hideEvent(event); }
