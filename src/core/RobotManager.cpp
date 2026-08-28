#include "RobotManager.h"
#include "RobotInstance.h"
#include "comm/BLEDeviceInfo.h"

RobotManager::RobotManager(QObject *parent)
    : QObject(parent)
{
}

RobotManager::~RobotManager() {
    qDeleteAll(m_robots);
    m_robots.clear();
}

RobotInstance *RobotManager::addRobot(const BLEDeviceInfo &info) {
    RobotId id = info.deriveId();
    if (m_robots.contains(id)) return m_robots[id];

    auto *robot = new RobotInstance(info, this);

    connect(robot, &RobotInstance::sensorDataUpdated, this, [this, id](const SensorData &data) {
        emit robotSensorDataUpdated(id, data);
    });
    connect(robot, &RobotInstance::connectionStateChanged, this, [this, id](RobotConnectionState s) {
        emit robotConnectionStateChanged(id, s);
    });
    connect(robot, &RobotInstance::poseChanged, this, [this, id](const Vec2 &pos, double heading) {
        emit robotPoseChanged(id, pos, heading);
    });

    m_robots[id] = robot;
    emit robotAdded(id);

    if (m_robots.size() == 1) {
        setSelectedRobot(id);
    }

    return robot;
}

void RobotManager::removeRobot(const RobotId &id) {
    if (!m_robots.contains(id)) return;
    delete m_robots.take(id);
    if (m_selectedId == id) {
        m_selectedId.clear();
        if (!m_robots.isEmpty()) setSelectedRobot(m_robots.firstKey());
    }
    emit robotRemoved(id);
}

RobotInstance *RobotManager::robot(const RobotId &id) const {
    return m_robots.value(id, nullptr);
}

QList<RobotInstance *> RobotManager::allRobots() const {
    return m_robots.values();
}

QList<RobotInstance *> RobotManager::connectedRobots() const {
    QList<RobotInstance *> result;
    for (auto *r : m_robots) {
        if (r->state() == RobotConnectionState::Connected)
            result.append(r);
    }
    return result;
}

int RobotManager::robotCount() const {
    return m_robots.size();
}

bool RobotManager::hasRobot(const RobotId &id) const {
    return m_robots.contains(id);
}

void RobotManager::setSelectedRobot(const RobotId &id) {
    if (m_selectedId != id && m_robots.contains(id)) {
        m_selectedId = id;
        emit selectedRobotChanged(id);
    }
}

RobotInstance *RobotManager::selectedRobot() const {
    return m_robots.value(m_selectedId, nullptr);
}
