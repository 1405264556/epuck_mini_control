#pragma once

#include "Types.h"
#include <QImage>
#include <QByteArray>
#include <QMap>
#include <QBluetoothUuid>
#include <array>

struct SensorData {
    enum Field { Proximity, AccelSpherical, Accelerometer, Gyroscope,
                 Microphone, ToF, Camera, Battery, Selector, Infrared, Encoders, FieldCount };
    TimestampMs timestamp = 0;
    uint32_t validFields = 0;
    std::array<TimestampMs, FieldCount> fieldTimestamps = {};

    void mark(Field field);
    bool has(Field field) const { return (validFields & (1u << field)) != 0; }
    bool isFresh(Field field, TimestampMs now, int maxAgeMs = 1000) const;
    void merge(const SensorData &update);

    // Proximity: 8 IR sensors, 0-4095 (closer = higher)
    std::array<uint16_t, 8> proximity = {};

    // IMU (cartesian)
    struct { float x = 0, y = 0, z = 0; } accelerometer;  // m/s^2 (raw cartesian)
    bool accelerometerInADC = false;
    struct { float x = 0, y = 0, z = 0; } gyroscope;       // deg/s

    // IMU (spherical — from official SerCom protocol CMD_ACCEL 0xBF)
    float accelMagnitude = 0;    // acceleration vector length
    float accelOrientation = 0;  // horizontal direction (0-360 deg, 0=front tilt down)
    float accelInclination = 0;  // angle from horizontal (0=flat, 90=vertical)

    // Microphone: 4 channels, normalized -1.0 to 1.0
    std::array<float, 4> microphones = {};
    int microphoneChannels = 4;
    // Raw microphone ADC values (from SerCom CMD_MIC 0xF5)
    std::array<uint16_t, 3> rawMicrophones = {};

    // ToF: VL53L5CX 8x8 zones, mm, row-major
    std::array<uint16_t, 64> tof = {};

    // Camera frame (may be empty if camera not streaming)
    QImage cameraFrame;

    // Battery voltage
    double batteryVoltage = 0.0;
    int batteryPercent = -1;
    std::array<int16_t, 2> encoders = {};

    // Selector position (0-15, from SerCom ASCII C command)
    int selectorPosition = -1;

    // IR remote control data
    uint8_t irCheck = 0;
    uint8_t irAddress = 0;
    uint8_t irData = 0;

    bool isValid() const { return timestamp != 0; }
    void clear();

    // Accumulate partial sensor updates into this struct.
    // Camera frames arrive chunked and are assembled here.
    void mergeFrom(const QBluetoothUuid &characteristic, const QByteArray &value);
};
