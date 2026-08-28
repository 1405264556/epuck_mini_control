#include "EpuckGATTProtocol.h"
#include <QDateTime>
#include <cstring>

// ---- Service UUIDs ----
const QBluetoothUuid EpuckGATTProtocol::SERVICE_CONTROL(
    "00001800-1212-efde-1523-785feabcd123");
const QBluetoothUuid EpuckGATTProtocol::SERVICE_SENSOR(
    "00001801-1212-efde-1523-785feabcd123");
const QBluetoothUuid EpuckGATTProtocol::SERVICE_OTA(
    "00001802-1212-efde-1523-785feabcd123");

// ---- Control characteristics ----
const QBluetoothUuid EpuckGATTProtocol::CHAR_MOTOR_SPEED(
    "00002a01-1212-efde-1523-785feabcd123");
const QBluetoothUuid EpuckGATTProtocol::CHAR_LED_CONTROL(
    "00002a02-1212-efde-1523-785feabcd123");
const QBluetoothUuid EpuckGATTProtocol::CHAR_BODY_LED(
    "00002a03-1212-efde-1523-785feabcd123");
const QBluetoothUuid EpuckGATTProtocol::CHAR_FRONT_LED(
    "00002a04-1212-efde-1523-785feabcd123");

// ---- Sensor characteristics ----
const QBluetoothUuid EpuckGATTProtocol::CHAR_PROXIMITY(
    "00002a10-1212-efde-1523-785feabcd123");
const QBluetoothUuid EpuckGATTProtocol::CHAR_IMU(
    "00002a11-1212-efde-1523-785feabcd123");
const QBluetoothUuid EpuckGATTProtocol::CHAR_MICROPHONE(
    "00002a12-1212-efde-1523-785feabcd123");
const QBluetoothUuid EpuckGATTProtocol::CHAR_TOF(
    "00002a13-1212-efde-1523-785feabcd123");
const QBluetoothUuid EpuckGATTProtocol::CHAR_CAMERA(
    "00002a14-1212-efde-1523-785feabcd123");
const QBluetoothUuid EpuckGATTProtocol::CHAR_BATTERY(
    quint16(0x2A19));

// ---- OTA characteristics ----
const QBluetoothUuid EpuckGATTProtocol::CHAR_OTA_CONTROL(
    "00002a20-1212-efde-1523-785feabcd123");
const QBluetoothUuid EpuckGATTProtocol::CHAR_OTA_DATA(
    "00002a21-1212-efde-1523-785feabcd123");
const QBluetoothUuid EpuckGATTProtocol::CHAR_OTA_STATUS(
    "00002a22-1212-efde-1523-785feabcd123");

// ---- Parsing ----
void EpuckGATTProtocol::parseSensorData(const QBluetoothUuid &characteristic,
                                         const QByteArray &value,
                                         SensorData &accumulator) {
    accumulator.timestamp = QDateTime::currentMSecsSinceEpoch();
    const auto *d = reinterpret_cast<const uint8_t *>(value.constData());
    int len = value.size();

    if (characteristic == CHAR_PROXIMITY) {
        for (int i = 0; i < 8 && (i * 2 + 1) < len; ++i) {
            accumulator.proximity[i] = static_cast<uint16_t>(d[i * 2] | (d[i * 2 + 1] << 8));
        }
    } else if (characteristic == CHAR_IMU) {
        if (len >= 24) {
            std::memcpy(&accumulator.accelerometer.x, d, 4);
            std::memcpy(&accumulator.accelerometer.y, d + 4, 4);
            std::memcpy(&accumulator.accelerometer.z, d + 8, 4);
            std::memcpy(&accumulator.gyroscope.x, d + 12, 4);
            std::memcpy(&accumulator.gyroscope.y, d + 16, 4);
            std::memcpy(&accumulator.gyroscope.z, d + 20, 4);
        }
    } else if (characteristic == CHAR_MICROPHONE) {
        for (int i = 0; i < 4 && (i * 4 + 3) < len; ++i) {
            std::memcpy(&accumulator.microphones[i], d + i * 4, 4);
        }
    } else if (characteristic == CHAR_TOF) {
        int count = (len - 1) / 2;
        for (int i = 0; i < count && i < 64; ++i) {
            accumulator.tof[i] = static_cast<uint16_t>(d[1 + i * 2] | (d[1 + i * 2 + 1] << 8));
        }
    } else if (characteristic == CHAR_BATTERY) {
        if (len >= 1) {
            accumulator.batteryVoltage = d[0] / 10.0;  // 0-255 -> 0-25.5V scaled
        }
    }
}

// ---- Encoding ----
QByteArray EpuckGATTProtocol::encodeMotorCommand(double leftSpeed, double rightSpeed) {
    auto toInt16 = [](double v) -> int16_t {
        double clamped = std::max(-1.0, std::min(1.0, v));
        return static_cast<int16_t>(clamped * 1000.0);
    };
    QByteArray data(4, '\0');
    int16_t left = toInt16(leftSpeed);
    int16_t right = toInt16(rightSpeed);
    data[0] = left & 0xFF;
    data[1] = (left >> 8) & 0xFF;
    data[2] = right & 0xFF;
    data[3] = (right >> 8) & 0xFF;
    return data;
}

QByteArray EpuckGATTProtocol::encodeLEDCommand(int index, uint8_t r, uint8_t g, uint8_t b) {
    QByteArray data(5, '\0');
    data[0] = static_cast<char>(index);
    data[1] = static_cast<char>(r);
    data[2] = static_cast<char>(g);
    data[3] = static_cast<char>(b);
    return data;
}

QByteArray EpuckGATTProtocol::encodeBodyLEDCommand(uint8_t r, uint8_t g, uint8_t b) {
    QByteArray data(3, '\0');
    data[0] = static_cast<char>(r);
    data[1] = static_cast<char>(g);
    data[2] = static_cast<char>(b);
    return data;
}

QByteArray EpuckGATTProtocol::encodeFrontLEDCommand(bool on) {
    QByteArray data(1, '\0');
    data[0] = on ? 1 : 0;
    return data;
}

QByteArray EpuckGATTProtocol::encodeOTAStartCommand(uint32_t firmwareSize, uint32_t crc32) {
    QByteArray data(8, '\0');
    data[0] = firmwareSize & 0xFF;
    data[1] = (firmwareSize >> 8) & 0xFF;
    data[2] = (firmwareSize >> 16) & 0xFF;
    data[3] = (firmwareSize >> 24) & 0xFF;
    data[4] = crc32 & 0xFF;
    data[5] = (crc32 >> 8) & 0xFF;
    data[6] = (crc32 >> 16) & 0xFF;
    data[7] = (crc32 >> 24) & 0xFF;
    return data;
}

QByteArray EpuckGATTProtocol::encodeOTADataChunk(uint32_t offset, const QByteArray &chunk) {
    QByteArray data(4 + chunk.size(), '\0');
    data[0] = offset & 0xFF;
    data[1] = (offset >> 8) & 0xFF;
    data[2] = (offset >> 16) & 0xFF;
    data[3] = (offset >> 24) & 0xFF;
    data.replace(4, chunk.size(), chunk);
    return data;
}

QByteArray EpuckGATTProtocol::encodeOTACommitCommand() {
    return QByteArray(1, '\x01');
}

// ---- Device filter ----
bool EpuckGATTProtocol::isEpuckMiniDevice(const QBluetoothDeviceInfo &info) {
    return isEpuckMiniName(info.name());
}

bool EpuckGATTProtocol::isEpuckMiniName(const QString &name) {
    return name.contains("e-puck", Qt::CaseInsensitive)
        || name.contains("epuck", Qt::CaseInsensitive);
}
