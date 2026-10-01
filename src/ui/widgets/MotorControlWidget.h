#pragma once
#include <QWidget>
#include "core/Types.h"
class QSlider;
class QPushButton;
class QTabWidget;
class QLabel;
class RobotManager;
class QTimer;
class QSpinBox;

// Joystick-style drive area (paint-based)
class JoystickWidget : public QWidget {
    Q_OBJECT
public:
    explicit JoystickWidget(QWidget *parent = nullptr);
    void reset();
signals:
    void speedsChanged(double left, double right);
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
private:
    void updateFromPos(const QPoint &pos);
    QPointF m_knobPos;
    bool m_dragging = false;
    static constexpr int AREA_SIZE = 200;
    static constexpr int KNOB_R = 16;
};

class MotorControlWidget : public QWidget {
    Q_OBJECT
public:
    explicit MotorControlWidget(RobotManager *robotMgr, QWidget *parent = nullptr);
    void setCurrentRobot(const RobotId &id);
    void releaseControl(bool sendStop = true);
signals:
    void manualCommandRequested(const RobotId &id, double left, double right);
protected:
    void hideEvent(QHideEvent *event) override;
private slots:
    void onSpeedChanged();
    void onStop();
    void onJoystickSpeed(double left, double right);
private:
    RobotManager *m_robotManager;
    RobotId m_currentId;
    QTabWidget *m_tabWidget;
    JoystickWidget *m_joystick;
    QSlider *m_leftSlider, *m_rightSlider;
    QLabel *m_leftLabel, *m_rightLabel;
    QPushButton *m_stopBtn;
    QSpinBox *m_leftSpin, *m_rightSpin;
    QTimer *m_heartbeat;
    WheelSpeeds m_activeSpeeds;
    bool m_moving = false;
    void sendSpeeds(double left, double right);
    void refreshEnabled();
};
