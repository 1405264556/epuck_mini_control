#pragma once

#include <QWidget>
#include "core/Types.h"

class QSlider;
class QSpinBox;
class QTableWidget;
class RobotManager;

class MultiRobotDriveWidget : public QWidget {
    Q_OBJECT
public:
    explicit MultiRobotDriveWidget(RobotManager *robotManager, QWidget *parent = nullptr);

private slots:
    void rebuildRobotRows();
    void sendGroupValues();
    void stopSelectedRobots();

private:
    QWidget *createValueControl(const QString &label, QSlider **slider, QSpinBox **spin,
                                int minimum, int maximum, int value);
    QList<RobotId> selectedRobotIds() const;
    void sendGroupMotion(int linear, int turn);
    void sendWheelValues(const RobotId &id, int left, int right);
    QString stateText(RobotConnectionState state) const;

    RobotManager *m_robotManager;
    QSlider *m_linearSlider = nullptr;
    QSlider *m_turnSlider = nullptr;
    QSpinBox *m_linearSpin = nullptr;
    QSpinBox *m_turnSpin = nullptr;
    QTableWidget *m_robotTable = nullptr;
};
