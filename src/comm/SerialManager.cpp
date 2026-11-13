#include "SerialManager.h"

#include "EpuckSerComProtocol.h"
#include "SerialPortWorker.h"
#include "MotorCommandScheduler.h"
#include "core/RobotInstance.h"
#include "core/RobotManager.h"
#include "util/Logger.h"

#include <QElapsedTimer>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

struct SerialScanSession {
    int token = 0;
    std::atomic_bool cancelled = false;
    QMutex reservationMutex;
    QSet<QString> reservedPorts;
};

namespace {
constexpr std::array<qint32, 3> kDefaultBaudRates{115200, 57600, 9600};

void configureProbePort(QSerialPort *port, const QString &portName, qint32 baudRate) {
    port->setPortName(portName);
    port->setBaudRate(baudRate);
    port->setDataBits(QSerialPort::Data8);
    port->setParity(QSerialPort::NoParity);
    port->setStopBits(QSerialPort::OneStop);
    port->setFlowControl(QSerialPort::NoFlowControl);
}

bool readUntil(QSerialPort &port, QByteArray &buffer, int timeoutMs, int minimumBytes = 0) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        const int remaining = qMax(1, timeoutMs - static_cast<int>(timer.elapsed()));
        if (!port.waitForReadyRead(remaining)) break;
        buffer.append(port.readAll());
        if (minimumBytes > 0 ? buffer.size() >= minimumBytes : buffer.contains('\n')) {
            return true;
        }
    }
    buffer.append(port.readAll());
    return minimumBytes > 0 ? buffer.size() >= minimumBytes : !buffer.isEmpty();
}

bool probePort(const QString &portName, const QList<int> &baudRates,
               const std::shared_ptr<SerialScanSession> &session, int &verifiedBaudRate) {
    auto shouldStop = [&]() {
        if (session->cancelled.load()) return true;
        QMutexLocker locker(&session->reservationMutex);
        return session->reservedPorts.contains(portName.toUpper());
    };
    for (const int baudRate : baudRates) {
        if (shouldStop()) return false;

        QSerialPort port;
        configureProbePort(&port, portName, baudRate);
        if (!port.open(QIODevice::ReadWrite)) continue;

        port.clear();
        port.write("\r");
        port.waitForBytesWritten(80);
        QThread::msleep(150);
        if (shouldStop()) return false;

        port.clear(QSerialPort::Input);
        port.write(EpuckSerComProtocol::buildSelectorQuery());
        port.waitForBytesWritten(80);
        QByteArray response;
        readUntil(port, response, 450);
        if (EpuckSerComProtocol::isSelectorResponse(response)) {
            verifiedBaudRate = baudRate;
            return true;
        }

        if (shouldStop()) return false;
        port.clear(QSerialPort::Input);
        const QByteArray sensorCommand = EpuckSerComProtocol::buildSensorPollCommand();
        port.write(sensorCommand);
        port.waitForBytesWritten(80);
        response.clear();
        readUntil(port, response, 500, 28);
        SensorData sample;
        if (response.size() == 28 && EpuckSerComProtocol::parseSensorResponse(response, sample)
            && sample.accelMagnitude >= 0 && sample.accelMagnitude < 100000
            && sample.accelOrientation >= 0 && sample.accelOrientation <= 360
            && sample.accelInclination >= -180 && sample.accelInclination <= 180) {
            verifiedBaudRate = baudRate;
            return true;
        }
    }
    return false;
}

bool looksLikeRobotPort(const BLEDeviceInfo &info) {
    const QString text = (info.friendlyName + " " + info.bluetoothAddr).toLower();
    return text.contains("bluetooth") || text.contains("spp") || text.contains("e-puck")
        || text.contains("epuck") || text.contains("robot") || text.contains("stm")
        || text.contains("usb serial") || text.contains("ch340") || text.contains("cp210");
}
}

SerialManager::SerialManager(RobotManager *robotManager, QObject *parent)
    : QObject(parent), m_robotManager(robotManager)
{
    m_scanPool.setMaxThreadCount(qBound(2, QThread::idealThreadCount(), 4));
    m_scanPool.setExpiryTimeout(3000);
}

SerialManager::~SerialManager() {
    m_shuttingDown = true;
    cancelScan(false);
    m_scanPool.clear();
    m_scanPool.waitForDone(1600);

    m_reconnects.clear();
    const auto contexts = m_contexts;
    m_contexts.clear();
    // Ask every worker to stop first, then wait for all of them against one shared
    // deadline. A worker stuck inside a blocking Bluetooth open() must not be able
    // to hang application exit, so nothing here waits without a bound.
    for (const auto &ctx : contexts) {
        if (!ctx.thread || !ctx.worker) continue;
        disconnect(ctx.worker, nullptr, this, nullptr);
        if (!ctx.thread->isRunning()) continue;
        // quit() is issued from inside the worker thread, after closePort() has run.
        // Calling it from here right after queuing closePort() lets the event loop
        // exit before dispatching it, and the final motor stop would never be sent.
        QPointer<SerialPortWorker> worker(ctx.worker);
        QThread *thread = ctx.thread;
        QMetaObject::invokeMethod(ctx.worker, [worker, thread]() {
            if (worker) worker->closePort();
            thread->quit();
        }, Qt::QueuedConnection);
    }
    QElapsedTimer deadline;
    deadline.start();
    for (const auto &ctx : contexts) {
        if (!ctx.thread || !ctx.thread->isRunning()) continue;
        const int remaining = qMax(1, 2000 - static_cast<int>(deadline.elapsed()));
        if (!ctx.thread->wait(remaining)) {
            qCWarning(logSerial) << "Serial thread for" << ctx.portName
                                 << "did not stop in time; leaving it to process exit";
        }
    }
}

void SerialManager::setAutoReconnect(bool enabled, int maxAttempts) {
    m_autoReconnect = enabled;
    m_maxReconnectAttempts = qBound(1, maxAttempts, 10);
    if (enabled) return;
    // Turning it off also withdraws retries that are already waiting.
    const auto pending = m_reconnects.keys();
    m_reconnects.clear();
    for (const auto &id : pending) {
        if (m_contexts.contains(id)) continue;   // an attempt is in flight; it ends on its own
        if (auto *robot = m_robotManager->robot(id)) {
            robot->setConnectionState(RobotConnectionState::Error);
        }
        emit robotDisconnected(id);
    }
}

void SerialManager::rememberPort(const QString &portName, int baudRate) {
    if (!portName.isEmpty() && baudRate > 0) m_knownBaudRates[portName] = baudRate;
}

void SerialManager::setDefaultPollInterval(int intervalMs) {
    m_pollIntervalMs = qBound(40, intervalMs, 1000);
    for (auto it = m_contexts.cbegin(); it != m_contexts.cend(); ++it) {
        if (it->connected) startSensorPolling(it.key(), m_pollIntervalMs);
    }
}

QStringList SerialManager::availablePorts() const {
    QStringList ports;
    for (const auto &info : QSerialPortInfo::availablePorts()) ports.append(info.portName());
    return ports;
}

void SerialManager::scanPorts() {
    if (m_scanning) return;
    cancelScan(false);

    auto session = std::make_shared<SerialScanSession>();
    session->token = ++m_scanToken;
    m_scanSession = session;
    m_discoveredDevices.clear();
    m_pendingVerifications = 0;
    m_scanCompleted = 0;
    m_scanTotal = 0;
    m_scanning = true;
    emit scanStarted();

    QPointer<SerialManager> self(this);
    m_scanPool.start([self, session]() {
        QList<BLEDeviceInfo> devices;
        const auto ports = QSerialPortInfo::availablePorts();
        devices.reserve(ports.size());
        for (const auto &port : ports) {
            if (session->cancelled.load()) return;
            BLEDeviceInfo info;
            info.portName = port.portName();
            info.friendlyName = port.description().isEmpty()
                ? port.portName()
                : QString("%1 (%2)").arg(port.description(), port.portName());
            info.bluetoothAddr = QString("%1 %2").arg(port.description(), port.manufacturer());
            devices.append(info);
        }

        std::stable_sort(devices.begin(), devices.end(), [](const auto &left, const auto &right) {
            return looksLikeRobotPort(left) && !looksLikeRobotPort(right);
        });

        if (!self || session->cancelled.load()) return;
        QMetaObject::invokeMethod(self.data(), [self, session, devices]() {
            if (self) self->handlePortsEnumerated(session, devices);
        }, Qt::QueuedConnection);
    });
}

void SerialManager::handlePortsEnumerated(const std::shared_ptr<SerialScanSession> &session,
                                          const QList<BLEDeviceInfo> &devices) {
    if (session != m_scanSession || session->cancelled.load()) return;

    m_discoveredDevices = devices;
    for (const auto &device : devices) emit deviceDiscovered(device);

    QList<BLEDeviceInfo> toVerify;
    for (auto &device : m_discoveredDevices) {
        if (isPortActive(device.portName)) {
            device.isEpuckMini = true;
            device.baudRate = m_knownBaudRates.value(device.portName, 115200);
            emit deviceDiscovered(device);
        } else {
            toVerify.append(device);
        }
    }

    m_pendingVerifications = toVerify.size();
    m_scanTotal = toVerify.size();
    if (toVerify.isEmpty()) {
        completeScan(session);
        return;
    }

    QPointer<SerialManager> self(this);
    for (const auto &device : toVerify) {
        QList<int> baudRates;
        const int knownBaud = m_knownBaudRates.value(device.portName, 0);
        if (knownBaud > 0) baudRates.append(knownBaud);
        for (const int baudRate : kDefaultBaudRates) {
            if (!baudRates.contains(baudRate)) baudRates.append(baudRate);
        }

        m_scanPool.start([self, session, device, baudRates]() {
            int verifiedBaud = 0;
            const bool verified = probePort(device.portName, baudRates, session, verifiedBaud);
            if (!self || session->cancelled.load()) return;
            QMetaObject::invokeMethod(self.data(),
                [self, session, portName = device.portName, verified, verifiedBaud]() {
                    if (self) self->handleProbeFinished(session, portName, verified, verifiedBaud);
                }, Qt::QueuedConnection);
        });
    }
}

void SerialManager::handleProbeFinished(const std::shared_ptr<SerialScanSession> &session,
                                        const QString &portName, bool verified, int baudRate) {
    if (session != m_scanSession || session->cancelled.load()) return;

    ++m_scanCompleted;
    --m_pendingVerifications;
    if (verified) {
        m_knownBaudRates[portName] = baudRate;
        for (auto &device : m_discoveredDevices) {
            if (device.portName != portName) continue;
            device.isEpuckMini = true;
            device.baudRate = baudRate;
            emit deviceDiscovered(device);
            break;
        }
        qCInfo(logSerial) << "Verified e-puck on" << portName << "at" << baudRate;
    }
    emit scanProgress(m_scanCompleted, m_scanTotal, portName, verified);

    if (m_pendingVerifications <= 0) completeScan(session);
}

void SerialManager::completeScan(const std::shared_ptr<SerialScanSession> &session) {
    if (session != m_scanSession || session->cancelled.load()) return;
    m_pendingVerifications = 0;
    m_scanning = false;
    m_scanSession.reset();
    qCInfo(logSerial) << "COM scan finished," << m_discoveredDevices.size() << "ports";
    emit scanFinished();
}

void SerialManager::stopScan() {
    cancelScan(true);
}

void SerialManager::cancelScan(bool notifyFinished) {
    const bool wasScanning = m_scanning;
    if (m_scanSession) m_scanSession->cancelled.store(true);
    m_scanSession.reset();
    m_scanPool.clear();
    m_pendingVerifications = 0;
    m_scanning = false;
    if (notifyFinished && wasScanning) emit scanFinished();
}

bool SerialManager::connectToPort(const QString &portName, const RobotId &id, int baudRate) {
    if (portName.trimmed().isEmpty() || id.isEmpty()) return false;
    if (m_contexts.contains(id)) return true;

    if (m_scanSession) {
        QMutexLocker locker(&m_scanSession->reservationMutex);
        m_scanSession->reservedPorts.insert(portName.toUpper());
    }

    for (auto it = m_contexts.cbegin(); it != m_contexts.cend(); ++it) {
        if (it.value().portName.compare(portName, Qt::CaseInsensitive) == 0) {
            emit errorOccurred(id, QString("端口 %1 已由 %2 使用").arg(portName, it.key()));
            return false;
        }
    }

    m_knownBaudRates[portName] = baudRate;
    BLEDeviceInfo info;
    info.portName = portName;
    info.friendlyName = portName;
    info.baudRate = baudRate;
    info.isEpuckMini = true;
    if (auto *existing = m_robotManager->robot(id)) {
        info.friendlyName = existing->name();
    }
    auto *robot = m_robotManager->addRobot(info);
    robot->setSerialManager(this);
    robot->setConnectionState(RobotConnectionState::Connecting);

    auto *thread = new QThread;
    auto *worker = new SerialPortWorker(id, portName, baudRate);
    worker->moveToThread(thread);

    PortContext context;
    context.portName = portName;
    context.baudRate = baudRate;
    context.thread = thread;
    context.worker = worker;
    m_contexts.insert(id, context);

    connect(thread, &QThread::started, worker, &SerialPortWorker::openPort);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(worker, &SerialPortWorker::connectionOpened, this, [this, worker](const RobotId &robotId) {
        auto it = m_contexts.find(robotId);
        if (it == m_contexts.end() || it->worker != worker || it->closing) return;
        it->connected = true;
        const bool recovered = m_reconnects.remove(robotId) > 0;
        if (m_pollIntervalMs != 60) startSensorPolling(robotId, m_pollIntervalMs);
        if (recovered) emit telemetryStatus(robotId, "已自动重新连接");
        if (auto *robot = m_robotManager->robot(robotId)) {
            robot->setConnectionState(RobotConnectionState::Connected);
        }
        emit robotConnected(robotId);
    });
    connect(worker, &SerialPortWorker::connectionClosed,
            this, &SerialManager::handleWorkerClosed);
    connect(worker, &SerialPortWorker::telemetryStatus, this, &SerialManager::telemetryStatus);
    connect(worker, &SerialPortWorker::sensorDataReceived, this,
            [this, worker](const RobotId &robotId, const SensorData &data) {
        const auto ctx = m_contexts.constFind(robotId);
        if (ctx == m_contexts.cend() || ctx->worker != worker || ctx->closing) return;
        if (auto *robot = m_robotManager->robot(robotId)) robot->updateSensorData(data);
        emit sensorDataReceived(robotId, data);
    });

    emit robotConnecting(id);
    thread->start();
    qCInfo(logSerial) << "Connecting to" << id << "on" << portName
                      << "using a dedicated serial thread";
    return true;
}

void SerialManager::disconnectRobot(const RobotId &id) {
    const bool cancelledReconnect = m_reconnects.remove(id) > 0;
    auto it = m_contexts.find(id);
    if (it == m_contexts.end() || !it->worker) {
        // Waiting between reconnect attempts: there is no worker, only the pending retry.
        if (cancelledReconnect) {
            if (auto *robot = m_robotManager->robot(id)) {
                robot->setConnectionState(RobotConnectionState::Disconnected);
            }
            emit robotDisconnected(id);
        }
        return;
    }
    it->closing = true;
    it->connected = false;
    if (auto *robot = m_robotManager->robot(id)) {
        robot->setConnectionState(RobotConnectionState::Disconnecting);
    }
    QMetaObject::invokeMethod(it->worker, &SerialPortWorker::closePort, Qt::QueuedConnection);
}

void SerialManager::handleWorkerClosed(const RobotId &id, const QString &reason) {
    auto it = m_contexts.find(id);
    if (it == m_contexts.end()) return;

    QThread *thread = it->thread;
    const QString portName = it->portName;
    const int baudRate = it->baudRate;
    const bool wasConnected = it->connected;
    const bool userClosing = it->closing;
    m_contexts.erase(it);
    if (thread) thread->quit();
    qCInfo(logSerial) << "Serial worker stopped for" << id << reason;
    if (m_shuttingDown) return;

    // Retry only links that were established (or are already being retried) and
    // that the user did not close. A first connection that never came up is
    // reported as a plain failure.
    const bool unexpected = !reason.isEmpty() && !userClosing;
    if (unexpected && m_autoReconnect && (wasConnected || m_reconnects.contains(id))
        && scheduleReconnect(id, portName, baudRate, reason)) {
        return;
    }

    const bool gaveUp = m_reconnects.remove(id) > 0;
    if (auto *robot = m_robotManager->robot(id)) {
        robot->setConnectionState(reason.isEmpty()
            ? RobotConnectionState::Disconnected
            : RobotConnectionState::Error);
    }
    if (!reason.isEmpty()) {
        emit errorOccurred(id, gaveUp
            ? QString("自动重连 %1 次均失败：%2").arg(m_maxReconnectAttempts).arg(reason)
            : reason);
    }
    emit robotDisconnected(id);
}

bool SerialManager::scheduleReconnect(const RobotId &id, const QString &portName, int baudRate,
                                      const QString &reason) {
    ReconnectState state = m_reconnects.value(id);
    if (state.attempt >= m_maxReconnectAttempts) return false;
    state.portName = portName;
    state.baudRate = baudRate;
    ++state.attempt;
    state.generation = ++m_reconnectGeneration;
    m_reconnects.insert(id, state);

    // 1 s, 2 s, 4 s ... capped: Bluetooth SPP ports need a moment before reopening.
    const int delayMs = qMin(8000, 1000 << (state.attempt - 1));
    if (auto *robot = m_robotManager->robot(id)) {
        // Leaving Connected stops coordination and zeroes the recorded command.
        robot->setConnectionState(RobotConnectionState::Connecting);
    }
    qCWarning(logSerial) << "Link lost for" << id << reason << "- retry" << state.attempt
                         << "of" << m_maxReconnectAttempts << "in" << delayMs << "ms";
    emit robotReconnecting(id, state.attempt, m_maxReconnectAttempts, delayMs);
    emit telemetryStatus(id, QString("连接中断，%1 秒后自动重连（第 %2/%3 次）")
                                 .arg(delayMs / 1000.0, 0, 'f', 0)
                                 .arg(state.attempt).arg(m_maxReconnectAttempts));

    const int generation = state.generation;
    QTimer::singleShot(delayMs, this, [this, id, generation]() {
        const auto pending = m_reconnects.constFind(id);
        // Cancelled by the user, superseded, or already reconnected by hand.
        if (m_shuttingDown || pending == m_reconnects.cend()
            || pending->generation != generation || m_contexts.contains(id)) return;
        if (!connectToPort(pending->portName, id, pending->baudRate)) {
            m_reconnects.remove(id);
            if (auto *robot = m_robotManager->robot(id)) {
                robot->setConnectionState(RobotConnectionState::Error);
            }
            emit robotDisconnected(id);
        }
    });
    return true;
}

bool SerialManager::isRobotConnected(const RobotId &id) const {
    const auto it = m_contexts.constFind(id);
    return it != m_contexts.cend() && it->connected;
}

bool SerialManager::isPortActive(const QString &portName) const {
    for (const auto &context : m_contexts) {
        if (context.portName.compare(portName, Qt::CaseInsensitive) == 0) return true;
    }
    // A port waiting for its next reconnect attempt must not be probed by a scan.
    for (const auto &pending : m_reconnects) {
        if (pending.portName.compare(portName, Qt::CaseInsensitive) == 0) return true;
    }
    return false;
}

bool SerialManager::writeToRobot(const RobotId &id, const QByteArray &data) {
    auto it = m_contexts.find(id);
    if (it == m_contexts.end() || !it->connected || !it->worker || data.isEmpty()) return false;
    QPointer<SerialPortWorker> worker(it->worker);
    if (static_cast<uint8_t>(data[0]) == EpuckSerComProtocol::CMD_MOTOR) {
        const quint64 sequence = ++m_motorSequence;
        const qint64 deadline = MotorCommandScheduler::nowMs() + 12;
        QMetaObject::invokeMethod(it->worker, [worker, data, deadline, sequence]() {
            if (worker) worker->scheduleMotorCommand(data, deadline, sequence);
        }, Qt::QueuedConnection);
        return true;
    }
    QMetaObject::invokeMethod(it->worker, [worker, data]() {
        if (worker) worker->writeData(data);
    }, Qt::QueuedConnection);
    return true;
}

quint64 SerialManager::writeMotorBatch(const QMap<RobotId, WheelSpeeds> &commands, int leadTimeMs) {
    const quint64 sequence = ++m_motorSequence;
    const qint64 deadline = MotorCommandScheduler::nowMs() + qBound(0, leadTimeMs, 100);
    int submitted = 0;
    for (auto it = commands.cbegin(); it != commands.cend(); ++it) {
        const auto context = m_contexts.constFind(it.key());
        if (context == m_contexts.cend() || !context->connected || context->closing) continue;
        const double left = std::isfinite(it->left) ? qBound(-1.0, it->left, 1.0) : 0;
        const double right = std::isfinite(it->right) ? qBound(-1.0, it->right, 1.0) : 0;
        const auto data = EpuckSerComProtocol::buildMotorCommand(qRound(left * 1000), qRound(right * 1000));
        if (auto *robot = m_robotManager->robot(it.key())) robot->recordMotorCommand(left, right);
        QPointer<SerialPortWorker> worker(context->worker);
        QMetaObject::invokeMethod(context->worker, [worker, data, deadline, sequence]() {
            if (worker) worker->scheduleMotorCommand(data, deadline, sequence);
        }, Qt::QueuedConnection);
        ++submitted;
    }
    if (submitted) emit motorBatchQueued(sequence, submitted, deadline);
    return sequence;
}

void SerialManager::startSensorPolling(const RobotId &id, int intervalMs) {
    auto it = m_contexts.find(id);
    if (it == m_contexts.end() || !it->worker) return;
    QPointer<SerialPortWorker> worker(it->worker);
    QMetaObject::invokeMethod(it->worker, [worker, intervalMs]() {
        if (worker) worker->startSensorPolling(intervalMs);
    }, Qt::QueuedConnection);
}

void SerialManager::stopSensorPolling(const RobotId &id) {
    auto it = m_contexts.find(id);
    if (it == m_contexts.end() || !it->worker) return;
    QMetaObject::invokeMethod(it->worker, &SerialPortWorker::stopSensorPolling,
                              Qt::QueuedConnection);
}

void SerialManager::requestCameraFrame(const RobotId &id) {
    auto it = m_contexts.find(id);
    if (it == m_contexts.end() || !it->connected || !it->worker) return;
    QMetaObject::invokeMethod(it->worker, &SerialPortWorker::requestCameraFrame,
                              Qt::QueuedConnection);
}
