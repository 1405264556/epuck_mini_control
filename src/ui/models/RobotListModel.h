#pragma once
#include <QAbstractListModel>
#include "core/Types.h"
class RobotManager;

class RobotListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { NameRole = Qt::DisplayRole, StateRole = Qt::UserRole + 1, BatteryRole, RSSIRole };

    explicit RobotListModel(RobotManager *manager, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    RobotId robotIdAt(int row) const;

public slots:
    void onRobotAdded(const RobotId &id);
    void onRobotRemoved(const RobotId &id);
    void onRobotStateChanged(const RobotId &id, RobotConnectionState state);

private:
    RobotManager *m_manager;
    QList<RobotId> m_robotIds;
};
