#include "Logger.h"
#include <QCoreApplication>
#include <QTextStream>

Q_LOGGING_CATEGORY(logApp, "app")
Q_LOGGING_CATEGORY(logBLE, "ble")
Q_LOGGING_CATEGORY(logControl, "control")
Q_LOGGING_CATEGORY(logDFU, "dfu")
Q_LOGGING_CATEGORY(logOTA, "ota")
Q_LOGGING_CATEGORY(logSerial, "serial")

Logger *Logger::instance() {
    static Logger inst;
    return &inst;
}

Logger::Logger() {
    qInstallMessageHandler(messageHandler);
}

Logger::~Logger() {
    qInstallMessageHandler(nullptr);
    if (m_logFile.isOpen()) m_logFile.close();
}

void Logger::setLogFile(const QString &path) {
    QMutexLocker lock(&m_mutex);
    if (m_logFile.isOpen()) m_logFile.close();
    m_logFile.setFileName(path);
    m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void Logger::setMinimumLevel(QtMsgType level) {
    QMutexLocker lock(&m_mutex);
    m_minLevel = level;
}

void Logger::messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg) {
    auto *self = instance();
    QMutexLocker lock(&self->m_mutex);

    if (type < self->m_minLevel) return;

    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = type;
    entry.message = msg;

    if (ctx.category) {
        entry.category = QString::fromUtf8(ctx.category);
    }

    self->m_entries.append(entry);
    while (self->m_entries.size() > self->m_maxEntries)
        self->m_entries.removeFirst();

    // Write to file
    if (self->m_logFile.isOpen()) {
        QTextStream stream(&self->m_logFile);
        stream << entry.timestamp.toString(Qt::ISODateWithMs)
               << " [" << entry.category << "] " << msg << "\n";
        stream.flush();
    }

    // Emit signal (unlock before emit to avoid deadlock)
    lock.unlock();
    emit self->newLogEntry(entry);
}
