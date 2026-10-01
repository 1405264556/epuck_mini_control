#include "RobotListModel.h"
#include "core/RobotManager.h"
#include "core/RobotInstance.h"

RobotListModel::RobotListModel(RobotManager *manager, QObject *parent)
    : QAbstractListModel(parent), m_manager(manager)
{
    for (auto *robot : m_manager->allRobots()) m_robotIds.append(robot->id());
    connect(m_manager, &RobotManager::robotAdded, this, &RobotListModel::onRobotAdded);
    connect(m_manager, &RobotManager::robotRemoved, this, &RobotListModel::onRobotRemoved);
    connect(m_manager, &RobotManager::robotConnectionStateChanged,
            this, &RobotListModel::onRobotStateChanged);
}

int RobotListModel::rowCount(const QModelIndex &) const { return m_robotIds.size(); }

QVariant RobotListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_robotIds.size()) return {};
    RobotId id = m_robotIds[index.row()];
    auto *r = m_manager->robot(id);
    if (!r) return {};
    switch (role) {
    case NameRole: return r->name();
    case StateRole: return static_cast<int>(r->state());
    case BatteryRole: return r->latestSensorData().batteryVoltage;
    case RSSIRole: return r->deviceInfo().rssi;
    default: return {};
    }
}

RobotId RobotListModel::robotIdAt(int row) const {
    return (row >= 0 && row < m_robotIds.size()) ? m_robotIds[row] : RobotId();
}

void RobotListModel::onRobotAdded(const RobotId &id) {
    beginInsertRows({}, m_robotIds.size(), m_robotIds.size());
    m_robotIds.append(id);
    endInsertRows();
}

void RobotListModel::onRobotRemoved(const RobotId &id) {
    int row = m_robotIds.indexOf(id);
    if (row >= 0) {
        beginRemoveRows({}, row, row);
        m_robotIds.removeAt(row);
        endRemoveRows();
    }
}

void RobotListModel::onRobotStateChanged(const RobotId &id, RobotConnectionState) {
    int row = m_robotIds.indexOf(id);
    if (row >= 0) emit dataChanged(index(row), index(row));
}
