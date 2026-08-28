#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QThreadPool>
#include <memory>

#include "BLEDeviceInfo.h"
#include "IDeviceComm.h"
#include "core/SensorData.h"
#include "core/Types.h"

class QThread;
class RobotManager;
class SerialPortWorker;
struct SerialScanSession;

class SerialManager : public QObject {
    Q_OBJECT
public:
    explicit SerialManager(RobotManager *robotManager, QObject *parent = nullptr);
    ~SerialManager() override;

    void scanPorts();
    void stopScan();
    bool isScanning() const { return m_scanning; }
    QStringList availablePorts() const;

    bool connectToPort(const QString &portName, const RobotId &id, int baudRate = 115200);
    void disconnectRobot(const RobotId &id);
    bool isRobotConnected(const RobotId &id) const;
    bool isPortActive(const QString &portName) const;

    bool writeToRobot(const RobotId &id, const QByteArray &data);
    void startSensorPolling(const RobotId &id, int intervalMs = 60);
    void stopSensorPolling(const RobotId &id);
    void requestCameraFrame(const RobotId &id);

signals:
    void scanStarted();
    void scanProgress(int completed, int total, const QString &portName, bool verified);
    void deviceDiscovered(const BLEDeviceInfo &info);
    void scanFinished();
    void robotConnecting(const RobotId &id);
    void robotConnected(const RobotId &id);
    void robotDisconnected(const RobotId &id);
    void sensorDataReceived(const RobotId &id, const SensorData &data);
    void errorOccurred(const RobotId &id, const QString &message);

private:
    struct PortContext {
        QString portName;
        int baudRate = 115200;
        QThread *thread = nullptr;
        SerialPortWorker *worker = nullptr;
        bool connected = false;
    };

    void cancelScan(bool notifyFinished);
    void handlePortsEnumerated(const std::shared_ptr<SerialScanSession> &session,
                               const QList<BLEDeviceInfo> &devices);
    void handleProbeFinished(const std::shared_ptr<SerialScanSession> &session,
                             const QString &portName, bool verified, int baudRate);
    void completeScan(const std::shared_ptr<SerialScanSession> &session);
    void handleWorkerClosed(const RobotId &id, const QString &reason);

    QMap<RobotId, PortContext> m_contexts;
    QList<BLEDeviceInfo> m_discoveredDevices;
    QMap<QString, int> m_knownBaudRates;
    std::shared_ptr<SerialScanSession> m_scanSession;
    QThreadPool m_scanPool;
    int m_scanToken = 0;
    int m_pendingVerifications = 0;
    int m_scanCompleted = 0;
    int m_scanTotal = 0;
    bool m_scanning = false;
    bool m_shuttingDown = false;
    RobotManager *m_robotManager;
};
