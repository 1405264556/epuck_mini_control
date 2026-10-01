#include "RobotInstance.h"
#include "comm/EpuckSerComProtocol.h"
#include "comm/SerialManager.h"
#include "util/MathUtils.h"
#include <cmath>

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
    if (!std::isfinite(left) || !std::isfinite(right)) { left = 0; right = 0; }
    recordMotorCommand(left, right);
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

void RobotInstance::recordMotorCommand(double left, double right) {
    m_commandedSpeeds = {qBound(-1.0, left, 1.0), qBound(-1.0, right, 1.0)};
}

void RobotInstance::setLED(int index, int value) {
    if (m_serialManager) {
        QByteArray cmd = EpuckSerComProtocol::buildLEDCommand(index, value);
        m_serialManager->writeToRobot(m_id, cmd);
    }
}

void RobotInstance::setBodyLED(int value) {
    if (m_serialManager) {
        QByteArray cmd = EpuckSerComProtocol::buildLEDCommand(8, value);
        m_serialManager->writeToRobot(m_id, cmd);
    }
}

void RobotInstance::setFrontLED(bool on) {
    if (m_serialManager) {
        QByteArray cmd = EpuckSerComProtocol::buildLEDCommand(9, on ? 1 : 0);
        m_serialManager->writeToRobot(m_id, cmd);
    }
}

void RobotInstance::stop() {
    recordMotorCommand(0, 0);
    if (m_serialManager) {
        QByteArray cmd = EpuckSerComProtocol::buildMotorCommand(0, 0);
        m_serialManager->writeToRobot(m_id, cmd);
    }
}

void RobotInstance::updateSensorData(const SensorData &data) {
    if (data.has(SensorData::Encoders)
        && data.fieldTimestamps[SensorData::Encoders] > m_latestSensorData.fieldTimestamps[SensorData::Encoders]) {
        if (m_haveEncoders) {
            // Signed 16-bit counters wrap; integrate measured steps, not requested speed.
            const int leftSteps = static_cast<int16_t>(static_cast<uint16_t>(data.encoders[0])
                - static_cast<uint16_t>(m_previousEncoders[0]));
            const int rightSteps = static_cast<int16_t>(static_cast<uint16_t>(data.encoders[1])
                - static_cast<uint16_t>(m_previousEncoders[1]));
            constexpr double cmPerStep = 4.1 * 3.141592653589793 / 1000.0;
            constexpr double wheelBaseCm = 5.3;
            if (std::abs(leftSteps) < 3000 && std::abs(rightSteps) < 3000) {
                const double distance = (leftSteps + rightSteps) * cmPerStep / 2.0;
                const double angle = (rightSteps - leftSteps) * cmPerStep / wheelBaseCm;
                const double middle = m_heading + angle / 2.0;
                setPosition(m_position + Vec2(distance * std::cos(middle), distance * std::sin(middle)),
                            math::normalizeAngle(m_heading + angle));
            }
        }
        m_previousEncoders = data.encoders;
        m_haveEncoders = true;
    }
    m_latestSensorData.merge(data);
    emit sensorDataUpdated(m_latestSensorData);
}

void RobotInstance::setConnectionState(RobotConnectionState state) {
    if (m_state != state) {
        m_state = state;
        if (state != RobotConnectionState::Connected) {
            m_haveEncoders = false;
            recordMotorCommand(0, 0);
        }
        if (state == RobotConnectionState::Connecting) m_latestSensorData.clear();
        emit connectionStateChanged(m_state);
    }
}
