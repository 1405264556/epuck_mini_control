#pragma once

#include <QWidget>
#include <QMap>
#include "core/Types.h"

class QSlider;
class QSpinBox;
class QTableWidget;
class RobotManager;
class MultiRobotCoordinator;
class QTimer;
class QLabel;

class MultiRobotDriveWidget : public QWidget {
    Q_OBJECT
public:
    explicit MultiRobotDriveWidget(RobotManager *robotManager, MultiRobotCoordinator *coordinator, QWidget *parent = nullptr);
    QList<RobotId> selectedRobotIds() const;
    void stopManualControl();
protected:
    void hideEvent(QHideEvent *event) override;

private slots:
    void rebuildRobotRows();
    void sendGroupValues();
    void stopSelectedRobots();

private:
    QWidget *createValueControl(const QString &label, QSlider **slider, QSpinBox **spin,
                                int minimum, int maximum, int value);
    void refreshRows();
    void dispatch(const QMap<RobotId, WheelSpeeds> &commands);
    void sendGroupMotion(int linear, int turn);
    void sendWheelValues(const RobotId &id, int left, int right);
    QString stateText(RobotConnectionState state) const;

    RobotManager *m_robotManager;
    MultiRobotCoordinator *m_coordinator;
    QMap<RobotId, WheelSpeeds> m_activeCommands;
    QList<RobotId> m_heldTargets;
    QTimer *m_heartbeat;
    QLabel *m_groupStatus;
    QSlider *m_linearSlider = nullptr;
    QSlider *m_turnSlider = nullptr;
    QSpinBox *m_linearSpin = nullptr;
    QSpinBox *m_turnSpin = nullptr;
    QTableWidget *m_robotTable = nullptr;
};
