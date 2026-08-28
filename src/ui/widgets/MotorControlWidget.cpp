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

// ==================== JoystickWidget ====================

JoystickWidget::JoystickWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(AREA_SIZE + 40, AREA_SIZE + 40);
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
    p.setPen(QPen(QColor("#585b70"), 2));
    p.setBrush(QColor("#313244"));
    p.drawEllipse(ox, oy, AREA_SIZE, AREA_SIZE);

    // Crosshair
    p.setPen(QPen(QColor("#45475a"), 1, Qt::DashLine));
    p.drawLine(ox, oy + AREA_SIZE / 2, ox + AREA_SIZE, oy + AREA_SIZE / 2);
    p.drawLine(ox + AREA_SIZE / 2, oy, ox + AREA_SIZE / 2, oy + AREA_SIZE);

    // Center dot
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#585b70"));
    p.drawEllipse(QPointF(ox + AREA_SIZE / 2.0, oy + AREA_SIZE / 2.0), 4, 4);

    // Knob position
    double kx = ox + m_knobPos.x();
    double ky = oy + m_knobPos.y();
    p.setPen(QPen(QColor("#89b4fa"), 2));
    p.setBrush(QColor("#89b4fa"));
    p.drawEllipse(QPointF(kx, ky), KNOB_R, KNOB_R);

    // Direction labels
    p.setPen(QColor("#a6adc8"));
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
    m_stopBtn->setStyleSheet("QPushButton#emergencyStop { background-color: #f38ba8; color: #1e1e2e; font-weight: bold; padding: 10px; }");
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

    m_tabWidget->addTab(sliderTab, "滑块");
    layout->addWidget(m_tabWidget);

    connect(m_leftSlider, &QSlider::valueChanged, this, &MotorControlWidget::onSpeedChanged);
    connect(m_rightSlider, &QSlider::valueChanged, this, &MotorControlWidget::onSpeedChanged);
    connect(m_stopBtn, &QPushButton::clicked, this, &MotorControlWidget::onStop);
    connect(m_joystick, &JoystickWidget::speedsChanged, this, &MotorControlWidget::onJoystickSpeed);
}

void MotorControlWidget::setCurrentRobot(const RobotId &id) { m_currentId = id; }

void MotorControlWidget::onSpeedChanged() {
    if (m_currentId.isEmpty()) return;
    m_leftLabel->setText(QString::number(m_leftSlider->value()));
    m_rightLabel->setText(QString::number(m_rightSlider->value()));
    auto *r = m_robotManager->robot(m_currentId);
    if (r) r->setMotorSpeeds(m_leftSlider->value() / 1000.0, m_rightSlider->value() / 1000.0);
}

void MotorControlWidget::onJoystickSpeed(double left, double right) {
    if (m_currentId.isEmpty()) return;
    auto *r = m_robotManager->robot(m_currentId);
    if (r) r->setMotorSpeeds(left / 1000.0, right / 1000.0);
}

void MotorControlWidget::onStop() {
    m_leftSlider->setValue(0);
    m_rightSlider->setValue(0);
    m_joystick->reset();
    if (auto *r = m_robotManager->robot(m_currentId)) r->stop();
}
