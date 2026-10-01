#pragma once
#include <QAbstractListModel>
#include "util/Logger.h"

class LogModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit LogModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex & = {}) const override { return m_entries.size(); }
    QVariant data(const QModelIndex &index, int role) const override;
public slots:
    void appendEntry(const LogEntry &entry);
private:
    QList<LogEntry> m_entries;
    static constexpr int MAX_VISIBLE = 1000;
};
