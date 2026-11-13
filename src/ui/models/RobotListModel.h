#pragma once
#include <QAbstractListModel>
#include <QMap>
#include "core/Types.h"
class RobotManager;

class RobotListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { NameRole = Qt::DisplayRole, StateRole = Qt::UserRole + 1, BatteryRole, RSSIRole,
                 PortRole, DetailRole };

    explicit RobotListModel(RobotManager *manager, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    RobotId robotIdAt(int row) const;
    int rowOf(const RobotId &id) const { return m_robotIds.indexOf(id); }
    // Extra status shown instead of the plain state text, e.g. reconnect progress.
    void setNote(const RobotId &id, const QString &note);
    static QString stateText(RobotConnectionState state);

public slots:
    void onRobotAdded(const RobotId &id);
    void onRobotRemoved(const RobotId &id);
    void onRobotStateChanged(const RobotId &id, RobotConnectionState state);

private:
    RobotManager *m_manager;
    QList<RobotId> m_robotIds;
    QMap<RobotId, QString> m_notes;
};
