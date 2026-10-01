#pragma once
#include <QObject>
#include <QTimer>
#include <QByteArray>

class MotorCommandScheduler : public QObject {
    Q_OBJECT
public:
    explicit MotorCommandScheduler(QObject *parent = nullptr);
    static qint64 nowMs();
    void schedule(const QByteArray &command, qint64 deadlineMs, quint64 sequence);
    void cancel();
signals:
    void commandReady(const QByteArray &command, quint64 sequence, qint64 sentAtMs);
private:
    QTimer m_timer;
    QByteArray m_pending;
    quint64 m_sequence = 0;
    qint64 m_deadline = 0;
};
