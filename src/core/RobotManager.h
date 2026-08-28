#pragma once

#include <QObject>
#include <QMap>
#include <QList>
#include "Types.h"
#include "SensorData.h"

class RobotInstance;
class BLEDeviceInfo;

class RobotManager : public QObject {
    Q_OBJECT
public:
    explicit RobotManager(QObject *parent = nullptr);
    ~RobotManager() override;

    RobotInstance *addRobot(const BLEDeviceInfo &info);
    void removeRobot(const RobotId &id);
    RobotInstance *robot(const RobotId &id) const;
    QList<RobotInstance *> allRobots() const;
    QList<RobotInstance *> connectedRobots() const;
    int robotCount() const;
    bool hasRobot(const RobotId &id) const;

    void setSelectedRobot(const RobotId &id);
    RobotId selectedRobotId() const { return m_selectedId; }
    RobotInstance *selectedRobot() const;

signals:
    void robotAdded(const RobotId &id);
    void robotRemoved(const RobotId &id);
    void robotSensorDataUpdated(const RobotId &id, const SensorData &data);
    void robotConnectionStateChanged(const RobotId &id, RobotConnectionState state);
    void robotPoseChanged(const RobotId &id, const Vec2 &pos, double heading);
    void selectedRobotChanged(const RobotId &id);

private:
    QMap<RobotId, RobotInstance *> m_robots;
    RobotId m_selectedId;
};
