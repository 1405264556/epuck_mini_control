#pragma once
#include <QWidget>
#include <QMap>
#include <array>
#include "core/Types.h"
class QPushButton;
class RobotManager;

class LEDControlWidget : public QWidget {
    Q_OBJECT
public:
    explicit LEDControlWidget(RobotManager *robotMgr, QWidget *parent = nullptr);
    void setCurrentRobot(const RobotId &id);
private slots:
    void onRingLEDClicked();
    void onBodyLEDClicked();
    void onFrontLEDClicked();
    void onAllOff();
private:
    void updateButtonStyle(QPushButton *btn, bool on);

    RobotManager *m_robotManager;
    RobotId m_currentId;
    QMap<RobotId, std::array<bool, 10>> m_requestedStates;

    // 8 ring LEDs (0-7, clockwise from front-right)
    QPushButton *m_ringLEDs[8];
    bool m_ringState[8] = {};

    // Body LED (8) and Front LED (9)
    QPushButton *m_bodyLED;
    QPushButton *m_frontLED;
    bool m_bodyOn = false;
    bool m_frontOn = false;
};
