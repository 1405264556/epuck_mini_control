#include "LogModel.h"
LogModel::LogModel(QObject *parent) : QAbstractListModel(parent) {}
QVariant LogModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_entries.size()) return {};
    if (role == Qt::DisplayRole) {
        auto &e = m_entries[index.row()];
        return QString("[%1] %2").arg(e.category, e.message);
    }
    return {};
}
void LogModel::appendEntry(const LogEntry &entry) {
    if (m_entries.size() >= MAX_VISIBLE) {
        beginRemoveRows({}, 0, 0);
        m_entries.removeFirst();
        endRemoveRows();
    }
    beginInsertRows({}, m_entries.size(), m_entries.size());
    m_entries.append(entry);
    endInsertRows();
}
