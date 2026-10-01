#pragma once

#include <QObject>
#include "Types.h"
#include "SensorData.h"
#include "comm/BLEDeviceInfo.h"

class SerialManager;

class RobotInstance : public QObject {
    Q_OBJECT
public:
    explicit RobotInstance(const BLEDeviceInfo &info, QObject *parent = nullptr);
    ~RobotInstance() override;

    RobotId id() const { return m_id; }
    QString name() const { return m_name; }
    RobotConnectionState state() const { return m_state; }
    const SensorData &latestSensorData() const { return m_latestSensorData; }
    const BLEDeviceInfo &deviceInfo() const { return m_deviceInfo; }
    void updateDeviceInfo(const BLEDeviceInfo &info) { m_deviceInfo = info; m_name = info.friendlyName; }

    // Position estimation (from odometry or user-set)
    Vec2 position() const { return m_position; }
    double heading() const { return m_heading; }
    WheelSpeeds commandedSpeeds() const { return m_commandedSpeeds; }
    void recordMotorCommand(double left, double right);
    void setPosition(const Vec2 &pos, double hdg);

    // Serial communication
    void setSerialManager(SerialManager *mgr) { m_serialManager = mgr; }
    SerialManager *serialManager() const { return m_serialManager; }

    // Control commands (via SerCom protocol over serial)
    void setMotorSpeeds(double left, double right);
    void setLED(int index, int value);
    void setBodyLED(int value);
    void setFrontLED(bool on);
    void stop();

    void updateSensorData(const SensorData &data);
    void setConnectionState(RobotConnectionState state);

signals:
    void sensorDataUpdated(const SensorData &data);
    void connectionStateChanged(RobotConnectionState state);
    void poseChanged(const Vec2 &pos, double heading);
    void errorOccurred(const QString &error);

private:
    RobotId m_id;
    QString m_name;
    BLEDeviceInfo m_deviceInfo;
    RobotConnectionState m_state = RobotConnectionState::Disconnected;
    SensorData m_latestSensorData;
    Vec2 m_position;
    double m_heading = 0.0;
    WheelSpeeds m_commandedSpeeds;
    bool m_haveEncoders = false;
    std::array<int16_t, 2> m_previousEncoders = {};

    SerialManager *m_serialManager = nullptr;

    // Sensor data accumulator for camera frame assembly
    QByteArray m_cameraBuffer;
    uint16_t m_cameraFrameId = 0;
    int m_cameraExpectedSize = 0;
};
