#include "SensorData.h"
#include <QDateTime>
#include "comm/EpuckGATTProtocol.h"

void SensorData::clear() {
    *this = SensorData{};
}

void SensorData::mark(Field field) {
    if (!timestamp) timestamp = QDateTime::currentMSecsSinceEpoch();
    validFields |= 1u << field;
    fieldTimestamps[field] = timestamp;
}

bool SensorData::isFresh(Field field, TimestampMs now, int maxAgeMs) const {
    const auto age = now - fieldTimestamps[field];
    return has(field) && age >= 0 && age <= maxAgeMs;
}

void SensorData::merge(const SensorData &d) {
    for (int i = 0; i < FieldCount; ++i) {
        const auto field = static_cast<Field>(i);
        if (!d.has(field) || d.fieldTimestamps[i] < fieldTimestamps[i]) continue;
        switch (field) {
        case Proximity: proximity = d.proximity; break;
        case AccelSpherical:
            accelMagnitude = d.accelMagnitude;
            accelOrientation = d.accelOrientation;
            accelInclination = d.accelInclination;
            break;
        case Accelerometer: accelerometer = d.accelerometer; accelerometerInADC = d.accelerometerInADC; break;
        case Gyroscope: gyroscope = d.gyroscope; break;
        case Microphone: microphones = d.microphones; rawMicrophones = d.rawMicrophones;
            microphoneChannels = d.microphoneChannels; break;
        case ToF: tof = d.tof; break;
        case Camera: cameraFrame = d.cameraFrame; break;
        case Battery: batteryVoltage = d.batteryVoltage; batteryPercent = d.batteryPercent; break;
        case Selector: selectorPosition = d.selectorPosition; break;
        case Infrared: irCheck = d.irCheck; irAddress = d.irAddress; irData = d.irData; break;
        case Encoders: encoders = d.encoders; break;
        case FieldCount: break;
        }
        validFields |= 1u << field;
        fieldTimestamps[i] = d.fieldTimestamps[i];
        timestamp = qMax(timestamp, d.fieldTimestamps[i]);
    }
}

void SensorData::mergeFrom(const QBluetoothUuid &characteristic, const QByteArray &value) {
    EpuckGATTProtocol::parseSensorData(characteristic, value, *this);
}
