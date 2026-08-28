#include "BLEManager.h"
#include "EpuckGATTProtocol.h"
#include "core/RobotManager.h"
#include "core/RobotInstance.h"
#include "util/Logger.h"
#include <QTimer>

BLEManager::BLEManager(RobotManager *robotManager, QObject *parent)
    : QObject(parent)
    , m_robotManager(robotManager)
    , m_discoveryAgent(new QBluetoothDeviceDiscoveryAgent(this))
    , m_discoveryTimer(new QTimer(this))
{
    m_discoveryTimer->setSingleShot(true);
    connect(m_discoveryTimer, &QTimer::timeout, this, [this]() {
        qCDebug(logBLE) << "BLE scan timeout, stopping...";
        m_discoveryAgent->stop();
    });

    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &BLEManager::onDeviceDiscovered);
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &BLEManager::onDiscoveryFinished);
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::errorOccurred,
            this, &BLEManager::onDiscoveryError);
}

BLEManager::~BLEManager() {
    for (auto &ctx : m_contexts) {
        if (ctx.controller) ctx.controller->disconnectFromDevice();
    }
    // Context controllers/services are Qt children; auto-deleted via parent
}

void BLEManager::startDiscovery(int timeoutMs) {
    m_discoveredDevices.clear();
    m_discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    m_discoveryTimer->start(timeoutMs);
    qCDebug(logBLE) << "BLE 扫描开始，超时:" << timeoutMs << "ms";
}

void BLEManager::stopDiscovery() {
    m_discoveryAgent->stop();
}

bool BLEManager::isDiscovering() const {
    return m_discoveryAgent->isActive();
}

QList<BLEDeviceInfo> BLEManager::discoveredDevices() const {
    return m_discoveredDevices;
}

void BLEManager::setFilterEpuckOnly(bool enabled) {
    m_filterEpuckOnly = enabled;
}

bool BLEManager::isFilterEpuckOnly() const {
    return m_filterEpuckOnly;
}

void BLEManager::connectToRobot(const RobotId &id, const BLEDeviceInfo &info) {
    if (m_contexts.contains(id)) {
        qCDebug(logBLE) << "Already have context for" << id << ", reconnecting...";
        auto *controller = m_contexts[id].controller;
        if (controller) {
            controller->connectToDevice();
            return;
        }
    }

    m_currentConnectingId = id;
    auto *controller = QLowEnergyController::createCentral(info.qtDeviceInfo, this);

    connect(controller, &QLowEnergyController::connected,
            this, &BLEManager::onConnected);
    connect(controller, &QLowEnergyController::disconnected,
            this, &BLEManager::onDisconnected);
    connect(controller, &QLowEnergyController::serviceDiscovered,
            this, &BLEManager::onServiceDiscovered);
    connect(controller, &QLowEnergyController::discoveryFinished,
            this, &BLEManager::onServiceDiscoveryFinished);
    connect(controller,
            QOverload<QLowEnergyController::Error>::of(&QLowEnergyController::errorOccurred),
            this, &BLEManager::onControllerError);

    RobotBLEContext ctx;
    ctx.controller = controller;
    m_contexts[id] = ctx;

    // Create RobotInstance in manager
    auto *robot = m_robotManager->addRobot(info);
    robot->setBleController(controller);
    robot->setConnectionState(RobotConnectionState::Connecting);

    controller->connectToDevice();
    qCDebug(logBLE) << "Connecting to robot" << id << info.friendlyName;
}

void BLEManager::disconnectRobot(const RobotId &id) {
    if (!m_contexts.contains(id)) return;
    auto &ctx = m_contexts[id];
    if (ctx.controller) {
        ctx.controller->disconnectFromDevice();
    }
    if (auto *robot = m_robotManager->robot(id)) {
        robot->setConnectionState(RobotConnectionState::Disconnecting);
    }
}

void BLEManager::writeCharacteristic(const RobotId &id, const QBluetoothUuid &service,
                                      const QBluetoothUuid &characteristic,
                                      const QByteArray &data) {
    if (!m_contexts.contains(id)) return;
    auto &ctx = m_contexts[id];
    QLowEnergyService *svc = nullptr;

    if (service == EpuckGATTProtocol::SERVICE_CONTROL) svc = ctx.controlService;
    else if (service == EpuckGATTProtocol::SERVICE_SENSOR) svc = ctx.sensorService;
    else if (service == EpuckGATTProtocol::SERVICE_OTA) svc = ctx.otaService;

    if (!svc) return;

    QLowEnergyCharacteristic ch = svc->characteristic(characteristic);
    if (ch.isValid()) {
        svc->writeCharacteristic(ch, data,
            ch.properties() & QLowEnergyCharacteristic::WriteNoResponse
                ? QLowEnergyService::WriteWithoutResponse
                : QLowEnergyService::WriteWithResponse);
    }
}

// ---- Private slots ----

void BLEManager::onDeviceDiscovered(const QBluetoothDeviceInfo &info) {
    bool isEpuck = EpuckGATTProtocol::isEpuckMiniDevice(info);

    if (m_filterEpuckOnly && !isEpuck) return;

    BLEDeviceInfo devInfo;
    devInfo.qtDeviceInfo = info;
    devInfo.friendlyName = info.name();
    devInfo.bluetoothAddr = info.address().toString();
    devInfo.rssi = info.rssi();
    devInfo.isEpuckMini = isEpuck;

    m_discoveredDevices.append(devInfo);
    emit deviceDiscovered(devInfo);
}

void BLEManager::onDiscoveryFinished() {
    m_discoveryTimer->stop();
    qCDebug(logBLE) << "BLE 扫描结束，发现" << m_discoveredDevices.size() << "台设备";
    emit discoveryFinished();
}

void BLEManager::onDiscoveryError(QBluetoothDeviceDiscoveryAgent::Error error) {
    qCWarning(logBLE) << "BLE discovery error:" << error;
    emit bleError(QString(), QString("扫描错误: %1").arg(error));
}

void BLEManager::onConnected() {
    RobotId id = m_currentConnectingId;
    qCDebug(logBLE) << "BLE connected to" << id;

    auto *robot = m_robotManager->robot(id);
    if (robot) robot->setConnectionState(RobotConnectionState::Connected);

    auto *controller = currentController();
    if (controller) {
        controller->discoverServices();
    }
    emit robotConnected(id);
}

void BLEManager::onDisconnected() {
    // Find which robot disconnected
    RobotId disconnectedId;
    for (auto it = m_contexts.begin(); it != m_contexts.end(); ++it) {
        if (it.value().controller == sender()) {
            disconnectedId = it.key();
            break;
        }
    }

    if (disconnectedId.isEmpty()) return;

    qCDebug(logBLE) << "BLE disconnected from" << disconnectedId;

    if (auto *robot = m_robotManager->robot(disconnectedId)) {
        robot->setConnectionState(RobotConnectionState::Disconnected);
    }
    emit robotDisconnected(disconnectedId);
}

void BLEManager::onServiceDiscovered(const QBluetoothUuid &newService) {
    auto *ctx = currentContext();
    if (!ctx) return;
    auto *controller = currentController();
    if (!controller) return;

    auto *service = controller->createServiceObject(newService, this);

    if (newService == EpuckGATTProtocol::SERVICE_CONTROL
        || newService == QBluetoothUuid::ServiceClassUuid::GenericAccess
        || newService == QBluetoothUuid::ServiceClassUuid::GenericAttribute) {
        ctx->controlService = service;
    } else if (newService == EpuckGATTProtocol::SERVICE_SENSOR) {
        ctx->sensorService = service;
    } else if (newService == EpuckGATTProtocol::SERVICE_OTA) {
        ctx->otaService = service;
    }

    connect(service, &QLowEnergyService::stateChanged,
            this, [this, service](QLowEnergyService::ServiceState state) {
        if (state == QLowEnergyService::RemoteServiceDiscovered) {
            connect(service, &QLowEnergyService::characteristicChanged,
                    this, &BLEManager::onCharacteristicChanged);
            connect(service,
                    QOverload<QLowEnergyService::ServiceError>::of(&QLowEnergyService::errorOccurred),
                    this, &BLEManager::onServiceError);

            // Enable notifications for sensor characteristics
            for (const auto &ch : service->characteristics()) {
                if (ch.properties() & QLowEnergyCharacteristic::Notify) {
                    QLowEnergyDescriptor cccd = ch.descriptor(
                        QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
                    if (cccd.isValid()) {
                        service->writeDescriptor(cccd, QByteArray::fromHex("0100"));
                    }
                }
            }
        }
    });

    service->discoverDetails();
}

void BLEManager::onServiceDiscoveryFinished() {
    auto *ctx = currentContext();
    if (ctx) {
        ctx->servicesDiscovered = true;
    }

    RobotId id = m_currentConnectingId;
    if (auto *robot = m_robotManager->robot(id)) {
        robot->setControlService(ctx->controlService);
        robot->setSensorService(ctx->sensorService);
        robot->setOTAService(ctx->otaService);
    }

    qCDebug(logBLE) << "Service discovery finished for" << id;
    emit robotServiceDiscoveryFinished(id);
}

void BLEManager::onServiceError(QLowEnergyService::ServiceError error) {
    qCWarning(logBLE) << "BLE service error:" << error;
}

void BLEManager::onCharacteristicChanged(const QLowEnergyCharacteristic &c,
                                          const QByteArray &value) {
    // Find the robot ID from the sender service
    auto *service = qobject_cast<QLowEnergyService *>(sender());
    if (!service) return;

    RobotId id;
    for (auto it = m_contexts.begin(); it != m_contexts.end(); ++it) {
        if (it.value().controlService == service
            || it.value().sensorService == service
            || it.value().otaService == service) {
            id = it.key();
            break;
        }
    }
    if (id.isEmpty()) return;

    // Determine service UUID
    QBluetoothUuid svcUuid = service->serviceUuid();

    emit characteristicValueChanged(id, svcUuid, c.uuid(), value);

    // Parse sensor data through Protocol and update RobotInstance
    if (auto *robot = m_robotManager->robot(id)) {
        SensorData data = robot->latestSensorData();
        EpuckGATTProtocol::parseSensorData(c.uuid(), value, data);
        robot->updateSensorData(data);
    }
}

void BLEManager::onControllerError(QLowEnergyController::Error error) {
    RobotId id = m_currentConnectingId;
    QString msg = QString("BLE 控制器错误: %1").arg(error);
    qCWarning(logBLE) << msg << "for" << id;

    if (auto *robot = m_robotManager->robot(id)) {
        robot->setConnectionState(RobotConnectionState::Error);
    }
    emit bleError(id, msg);
}

void BLEManager::startSensorPolling(const RobotId &id, int intervalMs) {
    Q_UNUSED(id); Q_UNUSED(intervalMs);
    // Sensor data arrives via BLE notifications (already subscribed in onServiceDiscovered).
    // This method exists for API symmetry with SerialManager.
}

void BLEManager::stopSensorPolling(const RobotId &id) {
    Q_UNUSED(id);
}

// ---- Helpers ----

QLowEnergyController *BLEManager::currentController() const {
    if (m_currentConnectingId.isEmpty() || !m_contexts.contains(m_currentConnectingId))
        return nullptr;
    return m_contexts[m_currentConnectingId].controller;
}

BLEManager::RobotBLEContext *BLEManager::currentContext() {
    if (m_currentConnectingId.isEmpty() || !m_contexts.contains(m_currentConnectingId))
        return nullptr;
    return &m_contexts[m_currentConnectingId];
}
