#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <array>

#include "core/SensorData.h"
#include "core/Types.h"

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

signals:
    void connectionOpened(const RobotId &id);
    void connectionClosed(const RobotId &id, const QString &reason);
    void sensorDataReceived(const RobotId &id, const SensorData &data);

private slots:
    void onReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);
    void pollSensors();

private:
    void initializeRobot();
    void processBuffer();
    void reportClosed(const QString &reason);

    RobotId m_id;
    QString m_portName;
    int m_baudRate;
    QSerialPort *m_port;
    QTimer *m_pollTimer;
    QByteArray m_readBuffer;
    bool m_connected = false;
    bool m_closedReported = false;
    int m_openAttempts = 0;
    int m_pollIntervalMs = 60;
    bool m_awaitingSensorFrame = false;
    QElapsedTimer m_sensorFrameTimer;
    bool m_capturingImage = false;
    int m_cameraExpectedSize = 0;
    QByteArray m_cameraBuffer;
    std::array<uint16_t, 8> m_filteredProximity = {};
    bool m_hasFilteredProximity = false;
};
