#include "RobotInstance.h"
#include "comm/EpuckSerComProtocol.h"
#include "comm/SerialManager.h"

RobotInstance::RobotInstance(const BLEDeviceInfo &info, QObject *parent)
    : QObject(parent)
    , m_id(info.deriveId())
    , m_name(info.friendlyName)
    , m_deviceInfo(info)
{
}

RobotInstance::~RobotInstance() = default;

void RobotInstance::setPosition(const Vec2 &pos, double hdg) {
    if (m_position.x == pos.x && m_position.y == pos.y && m_heading == hdg) return;
    m_position = pos;
    m_heading = hdg;
    emit poseChanged(m_position, m_heading);
}

void RobotInstance::setMotorSpeeds(double left, double right) {
    auto toInt16 = [](double v) -> int16_t {
        double clamped = (v < -1.0) ? -1.0 : (v > 1.0 ? 1.0 : v);
        return static_cast<int16_t>(clamped * 1000.0);
    };
    int16_t l = toInt16(left);
    int16_t r = toInt16(right);

    if (m_serialManager) {
        QByteArray cmd = EpuckSerComProtocol::buildMotorCommand(l, r);
        m_serialManager->writeToRobot(m_id, cmd);
    }
}

void RobotInstance::setLED(int index, int value) {
    if (m_serialManager) {
        QByteArray cmd = EpuckSerComProtocol::buildLEDCommand(index, value);
        m_serialManager->writeToRobot(m_id, cmd);
    }
}

void RobotInstance::setBodyLED(int value) {
    if (m_serialManager) {
        QByteArray cmd = EpuckSerComProtocol::buildBodyLEDAscii(value);
        m_serialManager->writeToRobot(m_id, cmd);
    }
}

void RobotInstance::setFrontLED(bool on) {
    if (m_serialManager) {
        QByteArray cmd = EpuckSerComProtocol::buildFrontLEDAscii(on ? 1 : 0);
        m_serialManager->writeToRobot(m_id, cmd);
    }
}

void RobotInstance::stop() {
    if (m_serialManager) {
        QByteArray cmd = EpuckSerComProtocol::buildMotorCommand(0, 0);
        m_serialManager->writeToRobot(m_id, cmd);
    }
}

void RobotInstance::updateSensorData(const SensorData &data) {
    m_latestSensorData.timestamp = data.timestamp;

    bool hasProximity = false;
    for (auto value : data.proximity) {
        if (value != 0) {
            hasProximity = true;
            break;
        }
    }
    if (hasProximity) {
        m_latestSensorData.proximity = data.proximity;
    }

    if (data.accelMagnitude != 0 || data.accelOrientation != 0 || data.accelInclination != 0) {
        m_latestSensorData.accelMagnitude = data.accelMagnitude;
        m_latestSensorData.accelOrientation = data.accelOrientation;
        m_latestSensorData.accelInclination = data.accelInclination;
    }

    if (data.accelerometer.x != 0 || data.accelerometer.y != 0 || data.accelerometer.z != 0) {
        m_latestSensorData.accelerometer = data.accelerometer;
    }

    bool hasMicrophones = false;
    for (auto value : data.rawMicrophones) {
        if (value != 0) {
            hasMicrophones = true;
            break;
        }
    }
    if (hasMicrophones) {
        m_latestSensorData.rawMicrophones = data.rawMicrophones;
    }

    if (data.selectorPosition >= 0) {
        m_latestSensorData.selectorPosition = data.selectorPosition;
    }
    if (data.irCheck != 0 || data.irAddress != 0 || data.irData != 0) {
        m_latestSensorData.irCheck = data.irCheck;
        m_latestSensorData.irAddress = data.irAddress;
        m_latestSensorData.irData = data.irData;
    }
    if (!data.cameraFrame.isNull()) {
        m_latestSensorData.cameraFrame = data.cameraFrame;
    }

    emit sensorDataUpdated(m_latestSensorData);
}

void RobotInstance::setConnectionState(RobotConnectionState state) {
    if (m_state != state) {
        m_state = state;
        emit connectionStateChanged(m_state);
    }
}
