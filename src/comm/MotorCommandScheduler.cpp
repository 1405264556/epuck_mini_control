#include "MotorCommandScheduler.h"
#include <chrono>

MotorCommandScheduler::MotorCommandScheduler(QObject *parent) : QObject(parent) {
    m_timer.setParent(this);
    m_timer.setSingleShot(true);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        const QByteArray command = m_pending;
        m_pending.clear();
        emit commandReady(command, m_sequence, nowMs());
    });
}

qint64 MotorCommandScheduler::nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void MotorCommandScheduler::schedule(const QByteArray &command, qint64 deadlineMs,
                                      quint64 sequence) {
    if (sequence < m_sequence || command.isEmpty()) return;
    m_sequence = sequence;
    m_pending = command;
    // Updating values retains the earlier send slot so rapid input cannot starve delivery.
    m_deadline = m_timer.isActive() ? qMin(m_deadline, deadlineMs) : deadlineMs;
    m_timer.start(static_cast<int>(qBound(qint64(0), m_deadline - nowMs(), qint64(1000))));
}

void MotorCommandScheduler::cancel() { m_timer.stop(); m_pending.clear(); }
