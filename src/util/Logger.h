#pragma once

#include <QObject>
#include <QString>
#include <QFile>
#include <QDateTime>
#include <QMutex>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logApp)
Q_DECLARE_LOGGING_CATEGORY(logBLE)
Q_DECLARE_LOGGING_CATEGORY(logControl)
Q_DECLARE_LOGGING_CATEGORY(logDFU)
Q_DECLARE_LOGGING_CATEGORY(logOTA)
Q_DECLARE_LOGGING_CATEGORY(logSerial)

struct LogEntry {
    QDateTime timestamp;
    QString category;
    QtMsgType level;
    QString message;
};

class Logger : public QObject {
    Q_OBJECT
public:
    static Logger *instance();

    void setLogFile(const QString &path);
    void setMinimumLevel(QtMsgType level);

    const QList<LogEntry> &entries() const { return m_entries; }
    int maxEntries() const { return m_maxEntries; }

signals:
    void newLogEntry(const LogEntry &entry);

private:
    Logger();
    ~Logger();
    static void messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg);

    QFile m_logFile;
    QtMsgType m_minLevel = QtDebugMsg;
    QList<LogEntry> m_entries;
    int m_maxEntries = 10000;
    QMutex m_mutex;
};
