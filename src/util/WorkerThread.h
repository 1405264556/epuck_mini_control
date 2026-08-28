#pragma once

#include <QThread>
#include <functional>
#include <type_traits>

template<typename T>
class WorkerThread {
public:
    using WorkFunc = std::function<T()>;
    using Callback = std::function<void(const T &)>;
    using ErrorCallback = std::function<void(const QString &)>;

    explicit WorkerThread(WorkFunc func, Callback onFinished = {},
                          ErrorCallback onError = {})
        : m_func(std::move(func))
        , m_onFinished(std::move(onFinished))
        , m_onError(std::move(onError)) {}

    ~WorkerThread() { cancel(); }

    void start() {
        cancel();
        m_thread = QThread::create([this]() {
            try {
                T result = m_func();
                if (m_onFinished) m_onFinished(result);
            } catch (const std::exception &e) {
                if (m_onError) m_onError(QString::fromUtf8(e.what()));
            }
        });
        QObject::connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
        m_thread->start();
    }

    void cancel() {
        if (m_thread && m_thread->isRunning()) {
            m_thread->requestInterruption();
            m_thread->wait(1000);
        }
    }

private:
    WorkFunc m_func;
    Callback m_onFinished;
    ErrorCallback m_onError;
    QThread *m_thread = nullptr;
};

// Void specialization
template<>
class WorkerThread<void> {
public:
    using WorkFunc = std::function<void()>;
    using Callback = std::function<void()>;
    using ErrorCallback = std::function<void(const QString &)>;

    explicit WorkerThread(WorkFunc func, Callback onFinished = {},
                          ErrorCallback onError = {})
        : m_func(std::move(func))
        , m_onFinished(std::move(onFinished))
        , m_onError(std::move(onError)) {}

    ~WorkerThread() { cancel(); }

    void start() {
        cancel();
        m_thread = QThread::create([this]() {
            try {
                m_func();
                if (m_onFinished) m_onFinished();
            } catch (const std::exception &e) {
                if (m_onError) m_onError(QString::fromUtf8(e.what()));
            }
        });
        QObject::connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
        m_thread->start();
    }

    void cancel() {
        if (m_thread && m_thread->isRunning()) {
            m_thread->requestInterruption();
            m_thread->wait(1000);
        }
    }

private:
    WorkFunc m_func;
    Callback m_onFinished;
    ErrorCallback m_onError;
    QThread *m_thread = nullptr;
};
