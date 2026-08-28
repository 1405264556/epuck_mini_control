#pragma once

#include <QObject>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QMap>
#include <QTimer>
#include "BLEDeviceInfo.h"
#include "core/Types.h"

class RobotManager;

class BLEManager : public QObject {
    Q_OBJECT
public:
    explicit BLEManager(RobotManager *robotManager, QObject *parent = nullptr);
    ~BLEManager() override;

    void startDiscovery(int timeoutMs = 10000);
    void stopDiscovery();
    bool isDiscovering() const;
    QList<BLEDeviceInfo> discoveredDevices() const;

    void setFilterEpuckOnly(bool enabled);
    bool isFilterEpuckOnly() const;

    void connectToRobot(const RobotId &id, const BLEDeviceInfo &info);
    void disconnectRobot(const RobotId &id);

    void writeCharacteristic(const RobotId &id, const QBluetoothUuid &service,
                             const QBluetoothUuid &characteristic, const QByteArray &data);

    void startSensorPolling(const RobotId &id, int intervalMs = 50);
    void stopSensorPolling(const RobotId &id);

signals:
    void deviceDiscovered(const BLEDeviceInfo &info);
    void discoveryFinished();
    void robotConnected(const RobotId &id);
    void robotDisconnected(const RobotId &id);
    void robotServiceDiscoveryFinished(const RobotId &id);
    void characteristicValueChanged(const RobotId &id, const QBluetoothUuid &service,
                                    const QBluetoothUuid &characteristic, const QByteArray &value);
    void bleError(const RobotId &id, const QString &message);

private slots:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &info);
    void onDiscoveryFinished();
    void onDiscoveryError(QBluetoothDeviceDiscoveryAgent::Error error);
    void onConnected();
    void onDisconnected();
    void onServiceDiscovered(const QBluetoothUuid &newService);
    void onServiceDiscoveryFinished();
    void onServiceError(QLowEnergyService::ServiceError error);
    void onCharacteristicChanged(const QLowEnergyCharacteristic &c, const QByteArray &value);
    void onControllerError(QLowEnergyController::Error error);

private:
    struct RobotBLEContext {
        QLowEnergyController *controller = nullptr;
        QLowEnergyService *controlService = nullptr;
        QLowEnergyService *sensorService = nullptr;
        QLowEnergyService *otaService = nullptr;
        bool servicesDiscovered = false;
        int reconnectAttempts = 0;
    };

    QLowEnergyController *currentController() const;
    RobotBLEContext *currentContext();

    RobotManager *m_robotManager;
    QBluetoothDeviceDiscoveryAgent *m_discoveryAgent;
    QTimer *m_discoveryTimer;
    QMap<RobotId, RobotBLEContext> m_contexts;
    QList<BLEDeviceInfo> m_discoveredDevices;
    RobotId m_currentConnectingId;
    bool m_filterEpuckOnly = true;
};
