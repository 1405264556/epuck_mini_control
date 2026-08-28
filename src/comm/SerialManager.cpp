#include "SerialManager.h"

#include "EpuckSerComProtocol.h"
#include "SerialPortWorker.h"
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
#include <algorithm>
#include <array>
#include <atomic>

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
        if (buffer.contains('\n') || (minimumBytes > 0 && buffer.size() >= minimumBytes)) {
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
        readUntil(port, response, 380, 16);
        if (response.startsWith(sensorCommand)) response.remove(0, sensorCommand.size());
        if (response.size() >= 16) {
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

    const auto contexts = m_contexts;
    m_contexts.clear();
    for (const auto &ctx : contexts) {
        if (!ctx.thread || !ctx.worker) continue;
        disconnect(ctx.worker, nullptr, this, nullptr);
        if (ctx.thread->isRunning()) {
            QMetaObject::invokeMethod(ctx.worker, &SerialPortWorker::closePort,
                                      Qt::BlockingQueuedConnection);
            ctx.thread->quit();
            ctx.thread->wait(1200);
        }
    }
}

QStringList SerialManager::availablePorts() const {
    QStringList ports;
    for (const auto &info : QSerialPortInfo::availablePorts()) ports.append(info.portName());
    return ports;
}

void SerialManager::scanPorts() {
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
    connect(worker, &SerialPortWorker::connectionOpened, this, [this](const RobotId &robotId) {
        auto it = m_contexts.find(robotId);
        if (it == m_contexts.end()) return;
        it->connected = true;
        if (auto *robot = m_robotManager->robot(robotId)) {
            robot->setConnectionState(RobotConnectionState::Connected);
        }
        emit robotConnected(robotId);
    });
    connect(worker, &SerialPortWorker::connectionClosed,
            this, &SerialManager::handleWorkerClosed);
    connect(worker, &SerialPortWorker::sensorDataReceived, this,
            [this](const RobotId &robotId, const SensorData &data) {
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
    auto it = m_contexts.find(id);
    if (it == m_contexts.end() || !it->worker) return;
    if (auto *robot = m_robotManager->robot(id)) {
        robot->setConnectionState(RobotConnectionState::Disconnecting);
    }
    QMetaObject::invokeMethod(it->worker, &SerialPortWorker::closePort, Qt::QueuedConnection);
}

void SerialManager::handleWorkerClosed(const RobotId &id, const QString &reason) {
    auto it = m_contexts.find(id);
    if (it == m_contexts.end()) return;

    QThread *thread = it->thread;
    m_contexts.erase(it);
    if (thread) thread->quit();

    if (!m_shuttingDown) {
        if (auto *robot = m_robotManager->robot(id)) {
            robot->setConnectionState(reason.isEmpty()
                ? RobotConnectionState::Disconnected
                : RobotConnectionState::Error);
        }
        if (!reason.isEmpty()) emit errorOccurred(id, reason);
        emit robotDisconnected(id);
    }
    qCInfo(logSerial) << "Serial worker stopped for" << id << reason;
}

bool SerialManager::isRobotConnected(const RobotId &id) const {
    const auto it = m_contexts.constFind(id);
    return it != m_contexts.cend() && it->connected;
}

bool SerialManager::isPortActive(const QString &portName) const {
    for (const auto &context : m_contexts) {
        if (context.portName.compare(portName, Qt::CaseInsensitive) == 0) return true;
    }
    return false;
}

bool SerialManager::writeToRobot(const RobotId &id, const QByteArray &data) {
    auto it = m_contexts.find(id);
    if (it == m_contexts.end() || !it->connected || !it->worker || data.isEmpty()) return false;
    QPointer<SerialPortWorker> worker(it->worker);
    QMetaObject::invokeMethod(it->worker, [worker, data]() {
        if (worker) worker->writeData(data);
    }, Qt::QueuedConnection);
    return true;
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
