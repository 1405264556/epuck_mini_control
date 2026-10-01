#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <array>

#include "core/SensorData.h"
#include "core/Types.h"
class MotorCommandScheduler;

class SerialPortWorker : public QObject {
    Q_OBJECT
public:
    SerialPortWorker(const RobotId &id, const QString &portName, int baudRate,
                     QObject *parent = nullptr);
    ~SerialPortWorker() override;

public slots:
    void openPort();
    void closePort();
    void writeData(const QByteArray &data);
    void startSensorPolling(int intervalMs = 60);
    void stopSensorPolling();
    void requestCameraFrame();
    void scheduleMotorCommand(const QByteArray &data, qint64 deadlineMs, quint64 sequence);

signals:
    void connectionOpened(const RobotId &id);
    void connectionClosed(const RobotId &id, const QString &reason);
    void sensorDataReceived(const RobotId &id, const SensorData &data);
    void telemetryStatus(const RobotId &id, const QString &message);

private slots:
    void onReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);
    void pollSensors();

private:
    void initializeRobot();
    void configureRobot();
    void processBuffer();
    void reportClosed(const QString &reason);
    void checkTimeouts();
    void finishCamera();
    void recoverTransaction(const QString &reason);

    RobotId m_id;
    QString m_portName;
    int m_baudRate;
    QSerialPort *m_port;
    QTimer *m_pollTimer;
    QTimer *m_healthTimer;
    MotorCommandScheduler *m_motorScheduler;
    QByteArray m_readBuffer;
    SensorData m_initialSensorData;
    bool m_connected = false;
    bool m_handshaking = false;
    bool m_binaryHandshake = false;
    bool m_closing = false;
    bool m_pollEnabled = true;
    bool m_closedReported = false;
    int m_openAttempts = 0;
    int m_pollIntervalMs = 60;
    bool m_awaitingSensorFrame = false;
    QElapsedTimer m_sensorFrameTimer;
    bool m_capturingImage = false;
    bool m_cameraQueued = false;
    bool m_recovering = false;
    bool m_extendedPolling = true;
    int m_sensorTimeouts = 0;
    int m_expectedSensorBytes = 44;
    QElapsedTimer m_cameraTimer;
    QElapsedTimer m_lastMotorTimer;
    QElapsedTimer m_recoveryTimer;
    bool m_motorMoving = false;
    int m_cameraExpectedSize = 0;
    QByteArray m_cameraBuffer;
    std::array<uint16_t, 8> m_filteredProximity = {};
    bool m_hasFilteredProximity = false;
};
