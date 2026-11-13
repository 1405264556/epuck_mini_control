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

// Joystick-style drive area. It scales with the widget and reports a normalised
// (forward, turn) pair; mapping to wheel speeds is left to the owner.
class JoystickWidget : public QWidget {
    Q_OBJECT
public:
    explicit JoystickWidget(QWidget *parent = nullptr);
    void reset();
    // Shows a command that came from elsewhere (keyboard) without emitting.
    void showCommand(double forward, double turn);
    bool isDragging() const { return m_dragging; }
    QSize sizeHint() const override { return {220, 190}; }
signals:
    void moved(double forward, double turn);
    void released();
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void changeEvent(QEvent *) override;
private:
    void updateFromPos(const QPointF &pos);
    double padRadius() const;
    QPointF padCenter() const;
    QPointF m_knob;          // unit disc, +x right, +y up
    bool m_dragging = false;
    static constexpr double kDeadzone = 0.08;
};

class MotorControlWidget : public QWidget {
    Q_OBJECT
public:
    explicit MotorControlWidget(RobotManager *robotMgr, QWidget *parent = nullptr);
    void setCurrentRobot(const RobotId &id);
    void releaseControl(bool sendStop = true);

    // Differential mix shared by joystick and keyboard. Inputs are in [-1, 1];
    // the result is scaled so neither wheel exceeds limit steps/s.
    static WheelSpeeds mixDrive(double forward, double turn, int limitStepsPerSecond);

    int speedLimit() const;
    void setSpeedLimit(int stepsPerSecond);
    // Keyboard (or any external) drive request for the current robot.
    void driveNormalized(double forward, double turn);
    bool isMoving() const { return m_moving; }
    // Further pages (LED, path planning) share this tab bar so the panel needs only
    // one row of tabs; the readout and stop button stay visible under all of them.
    void addExtraTab(QWidget *page, const QString &title);
    bool isJoystickTabActive() const;
    void showJoystickTab();
signals:
    void manualCommandRequested(const RobotId &id, double left, double right);
    void speedLimitChanged(int stepsPerSecond);
    void stopClicked();
protected:
    void hideEvent(QHideEvent *event) override;
private slots:
    void onSpeedChanged();
    void onStop();
private:
    RobotManager *m_robotManager;
    RobotId m_currentId;
    QTabWidget *m_tabWidget;
    QWidget *m_joystickPage;
    QWidget *m_wheelPage;
    JoystickWidget *m_joystick;
    QSlider *m_leftSlider, *m_rightSlider;
    QSlider *m_limitSlider;
    QSpinBox *m_limitSpin;
    QLabel *m_readout;
    QLabel *m_hint;
    QPushButton *m_stopBtn;
    QSpinBox *m_leftSpin, *m_rightSpin;
    QTimer *m_heartbeat;
    WheelSpeeds m_activeSpeeds;
    bool m_moving = false;
    void sendSpeeds(double left, double right);
    void refreshEnabled();
    void refreshReadout();
};
