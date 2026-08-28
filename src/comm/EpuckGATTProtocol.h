#pragma once

#include <QBluetoothUuid>
#include <QBluetoothDeviceInfo>
#include <QByteArray>
#include "core/SensorData.h"

class EpuckGATTProtocol {
public:
    // ---- GATT Service UUIDs (128-bit custom) ----
    static const QBluetoothUuid SERVICE_CONTROL;
    static const QBluetoothUuid SERVICE_SENSOR;
    static const QBluetoothUuid SERVICE_OTA;

    // ---- Control characteristics ----
    static const QBluetoothUuid CHAR_MOTOR_SPEED;
    static const QBluetoothUuid CHAR_LED_CONTROL;
    static const QBluetoothUuid CHAR_BODY_LED;
    static const QBluetoothUuid CHAR_FRONT_LED;

    // ---- Sensor characteristics ----
    static const QBluetoothUuid CHAR_PROXIMITY;
    static const QBluetoothUuid CHAR_IMU;
    static const QBluetoothUuid CHAR_MICROPHONE;
    static const QBluetoothUuid CHAR_TOF;
    static const QBluetoothUuid CHAR_CAMERA;
    static const QBluetoothUuid CHAR_BATTERY;

    // ---- OTA characteristics ----
    static const QBluetoothUuid CHAR_OTA_CONTROL;
    static const QBluetoothUuid CHAR_OTA_DATA;
    static const QBluetoothUuid CHAR_OTA_STATUS;

    // ---- Parsing ----
    static void parseSensorData(const QBluetoothUuid &characteristic,
                                const QByteArray &value,
                                SensorData &accumulator);

    // ---- Encoding ----
    static QByteArray encodeMotorCommand(double leftSpeed, double rightSpeed);
    static QByteArray encodeLEDCommand(int index, uint8_t r, uint8_t g, uint8_t b);
    static QByteArray encodeBodyLEDCommand(uint8_t r, uint8_t g, uint8_t b);
    static QByteArray encodeFrontLEDCommand(bool on);
    static QByteArray encodeOTAStartCommand(uint32_t firmwareSize, uint32_t crc32);
    static QByteArray encodeOTADataChunk(uint32_t offset, const QByteArray &chunk);
    static QByteArray encodeOTACommitCommand();

    // ---- Device filter ----
    static bool isEpuckMiniDevice(const QBluetoothDeviceInfo &info);
    static bool isEpuckMiniName(const QString &name);
};
