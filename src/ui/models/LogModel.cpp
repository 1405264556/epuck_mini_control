#include "LogModel.h"
#include <QColor>

LogModel::LogModel(QObject *parent) : QAbstractListModel(parent) {}

QVariant LogModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_entries.size()) return {};
    const auto &e = m_entries[index.row()];
    if (role == Qt::DisplayRole) {
        const QString time = e.timestamp.isValid() ? e.timestamp.toString("HH:mm:ss.zzz ") : QString();
        return QString("%1[%2] %3").arg(time, e.category, e.message);
    }
    if (role == Qt::ForegroundRole) {
        switch (e.level) {
        case QtWarningMsg: return QColor("#8a5a10");
        case QtCriticalMsg:
        case QtFatalMsg: return QColor("#b3323a");
        case QtDebugMsg: return QColor("#6f828a");
        default: return QColor("#223139");
        }
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
