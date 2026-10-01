#include "MotorControlWidget.h"
#include "core/RobotManager.h"
#include "core/RobotInstance.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QPushButton>
#include <QTabWidget>
#include <QLabel>
#include <QPainter>
#include <QMouseEvent>
#include <QtMath>
#include <QTimer>
#include <QSpinBox>
#include <QSignalBlocker>
#include <QStyle>
#include <QHideEvent>

// ==================== JoystickWidget ====================

JoystickWidget::JoystickWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(AREA_SIZE + 64, AREA_SIZE + 40);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::CrossCursor);
    m_knobPos = QPointF(AREA_SIZE / 2.0, AREA_SIZE / 2.0);
}

void JoystickWidget::reset() {
    m_knobPos = QPointF(AREA_SIZE / 2.0, AREA_SIZE / 2.0);
    update();
    emit speedsChanged(0, 0);
}

void JoystickWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int ox = (width() - AREA_SIZE) / 2;
    int oy = (height() - AREA_SIZE) / 2;

    // Background circle
    p.setPen(QPen(QColor("#b5c3c8"), 2));
    p.setBrush(QColor("#eef4f3"));
    p.drawEllipse(ox, oy, AREA_SIZE, AREA_SIZE);

    // Crosshair
    p.setPen(QPen(QColor("#c3d0d3"), 1, Qt::DashLine));
    p.drawLine(ox, oy + AREA_SIZE / 2, ox + AREA_SIZE, oy + AREA_SIZE / 2);
    p.drawLine(ox + AREA_SIZE / 2, oy, ox + AREA_SIZE / 2, oy + AREA_SIZE);

    // Center dot
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#879a9e"));
    p.drawEllipse(QPointF(ox + AREA_SIZE / 2.0, oy + AREA_SIZE / 2.0), 4, 4);

    // Knob position
    double kx = ox + m_knobPos.x();
    double ky = oy + m_knobPos.y();
    p.setPen(QPen(QColor("#087f72"), 2));
    p.setBrush(QColor("#087f72"));
    p.drawEllipse(QPointF(kx, ky), KNOB_R, KNOB_R);

    // Direction labels
    p.setPen(QColor("#53676d"));
    QFont f = p.font(); f.setPointSize(10); p.setFont(f);
    p.drawText(QRect(ox, oy - 20, AREA_SIZE, 20), Qt::AlignCenter, "前");
    p.drawText(QRect(ox, oy + AREA_SIZE, AREA_SIZE, 20), Qt::AlignCenter, "后");
    p.drawText(QRect(ox - 30, oy, 30, AREA_SIZE), Qt::AlignCenter, "左");
    p.drawText(QRect(ox + AREA_SIZE, oy, 30, AREA_SIZE), Qt::AlignCenter, "右");
}

void JoystickWidget::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
        updateFromPos(e->pos());
    }
}

void JoystickWidget::mouseMoveEvent(QMouseEvent *e) {
    if (m_dragging) updateFromPos(e->pos());
}

void JoystickWidget::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        m_knobPos = QPointF(AREA_SIZE / 2.0, AREA_SIZE / 2.0);
        update();
        emit speedsChanged(0, 0);
    }
}

void JoystickWidget::updateFromPos(const QPoint &pos) {
    int ox = (width() - AREA_SIZE) / 2;
    int oy = (height() - AREA_SIZE) / 2;
    QPointF center(ox + AREA_SIZE / 2.0, oy + AREA_SIZE / 2.0);

    double dx = pos.x() - center.x();
    double dy = pos.y() - center.y();
    double dist = qSqrt(dx * dx + dy * dy);
    double radius = AREA_SIZE / 2.0 - KNOB_R;

    if (dist > radius) {
        dx = dx / dist * radius;
        dy = dy / dist * radius;
    }

    m_knobPos = QPointF(AREA_SIZE / 2.0 + dx, AREA_SIZE / 2.0 + dy);
    update();

    // Convert to differential drive: forward = -dy (up is forward)
    double fwd = -dy / radius;
    double turn = dx / radius;
    double left = (fwd + turn) * 1000.0;
    double right = (fwd - turn) * 1000.0;
    left = qBound(-1000.0, left, 1000.0);
    right = qBound(-1000.0, right, 1000.0);

    emit speedsChanged(left, right);
}

// ==================== MotorControlWidget ====================

MotorControlWidget::MotorControlWidget(RobotManager *robotMgr, QWidget *parent)
    : QWidget(parent), m_robotManager(robotMgr)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_tabWidget = new QTabWidget;

    // ---- Tab 1: Joystick ----
    auto *joyTab = new QWidget;
    auto *joyLayout = new QVBoxLayout(joyTab);
    m_joystick = new JoystickWidget;
    joyLayout->addWidget(m_joystick);
    m_tabWidget->addTab(joyTab, "摇杆");

    // ---- Tab 2: Sliders ----
    auto *sliderTab = new QWidget;
    auto *hlay = new QHBoxLayout(sliderTab);

    auto *leftBox = new QVBoxLayout;
    leftBox->addWidget(new QLabel("左轮"));
    m_leftSlider = new QSlider(Qt::Vertical);
    m_leftSlider->setRange(-1000, 1000); m_leftSlider->setValue(0);
    leftBox->addWidget(m_leftSlider);
    m_leftLabel = new QLabel("0");
    leftBox->addWidget(m_leftLabel);
    hlay->addLayout(leftBox);

    auto *midBox = new QVBoxLayout;
    midBox->addStretch();
    m_stopBtn = new QPushButton("急停");
    m_stopBtn->setObjectName("emergencyStop");
    m_stopBtn->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    midBox->addWidget(m_stopBtn);
    midBox->addStretch();
    hlay->addLayout(midBox);

    auto *rightBox = new QVBoxLayout;
    rightBox->addWidget(new QLabel("右轮"));
    m_rightSlider = new QSlider(Qt::Vertical);
    m_rightSlider->setRange(-1000, 1000); m_rightSlider->setValue(0);
    rightBox->addWidget(m_rightSlider);
    m_rightLabel = new QLabel("0");
    rightBox->addWidget(m_rightLabel);
    hlay->addLayout(rightBox);
    m_leftSpin = new QSpinBox;
    m_rightSpin = new QSpinBox;
    for (auto *spin : {m_leftSpin, m_rightSpin}) {
        spin->setRange(-1000, 1000);
        spin->setSingleStep(25);
        spin->setKeyboardTracking(false);
        spin->setFixedWidth(82);
        spin->setToolTip("步 / 秒，范围 -1000 到 1000");
    }
    leftBox->addWidget(m_leftSpin);
    rightBox->addWidget(m_rightSpin);
    connect(m_leftSlider, &QSlider::valueChanged, m_leftSpin, &QSpinBox::setValue);
    connect(m_rightSlider, &QSlider::valueChanged, m_rightSpin, &QSpinBox::setValue);
    connect(m_leftSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_leftSlider, &QSlider::setValue);
    connect(m_rightSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_rightSlider, &QSlider::setValue);

    m_tabWidget->addTab(sliderTab, "滑块");
    layout->addWidget(m_tabWidget);

    connect(m_leftSlider, &QSlider::valueChanged, this, &MotorControlWidget::onSpeedChanged);
    connect(m_rightSlider, &QSlider::valueChanged, this, &MotorControlWidget::onSpeedChanged);
    connect(m_stopBtn, &QPushButton::clicked, this, &MotorControlWidget::onStop);
    connect(m_joystick, &JoystickWidget::speedsChanged, this, &MotorControlWidget::onJoystickSpeed);
    m_heartbeat = new QTimer(this);
    m_heartbeat->setInterval(100);
    connect(m_heartbeat, &QTimer::timeout, this, [this]() {
        if (m_moving && isVisible()) sendSpeeds(m_activeSpeeds.left, m_activeSpeeds.right);
    });
    m_heartbeat->start();
    connect(m_robotManager, &RobotManager::robotConnectionStateChanged, this,
            [this](const RobotId &id, RobotConnectionState) { if (id == m_currentId) refreshEnabled(); });
    refreshEnabled();
}

void MotorControlWidget::setCurrentRobot(const RobotId &id) {
    if (id != m_currentId) releaseControl();
    m_currentId = id;
    refreshEnabled();
}

void MotorControlWidget::onSpeedChanged() {
    if (m_currentId.isEmpty()) return;
    m_leftLabel->setText(QString::number(m_leftSlider->value()));
    m_rightLabel->setText(QString::number(m_rightSlider->value()));
    sendSpeeds(m_leftSlider->value() / 1000.0, m_rightSlider->value() / 1000.0);
}

void MotorControlWidget::onJoystickSpeed(double left, double right) {
    if (m_currentId.isEmpty()) return;
    sendSpeeds(left / 1000.0, right / 1000.0);
}

void MotorControlWidget::onStop() {
    sendSpeeds(0, 0);
    releaseControl(false);
}

void MotorControlWidget::sendSpeeds(double left, double right) {
    auto *robot = m_robotManager->robot(m_currentId);
    if (!robot || robot->state() != RobotConnectionState::Connected) return;
    m_activeSpeeds = {left, right};
    m_moving = left != 0 || right != 0;
    emit manualCommandRequested(m_currentId, left, right);
}

void MotorControlWidget::releaseControl(bool sendStop) {
    if (sendStop && m_moving) sendSpeeds(0, 0);
    m_moving = false;
    m_activeSpeeds = {};
    const QSignalBlocker left(m_leftSlider), right(m_rightSlider), joystick(m_joystick);
    m_leftSlider->setValue(0); m_rightSlider->setValue(0);
    m_leftSpin->setValue(0); m_rightSpin->setValue(0);
    m_leftLabel->setText("0"); m_rightLabel->setText("0");
    m_joystick->reset();
}

void MotorControlWidget::refreshEnabled() {
    auto *r = m_robotManager->robot(m_currentId);
    const bool online = r && r->state() == RobotConnectionState::Connected;
    if (!online) releaseControl(false);
    m_tabWidget->setEnabled(online);
}
void MotorControlWidget::hideEvent(QHideEvent *event) { releaseControl(); QWidget::hideEvent(event); }
