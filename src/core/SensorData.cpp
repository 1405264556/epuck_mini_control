#include "SensorData.h"
#include <QDateTime>
#include <cstring>

void SensorData::clear() {
    timestamp = 0;
    proximity.fill(0);
    accelerometer = {};
    gyroscope = {};
    accelMagnitude = 0;
    accelOrientation = 0;
    accelInclination = 0;
    microphones.fill(0);
    rawMicrophones.fill(0);
    tof.fill(0);
    cameraFrame = {};
    batteryVoltage = 0.0;
    selectorPosition = -1;
    irCheck = 0;
    irAddress = 0;
    irData = 0;
}

void SensorData::mergeFrom(const QBluetoothUuid & /*characteristic*/, const QByteArray &value) {
    timestamp = QDateTime::currentMSecsSinceEpoch();
    const auto *d = reinterpret_cast<const uint8_t *>(value.constData());
    int len = value.size();
    if (len < 2) return;

    // Sensor data routing is handled by EpuckGATTProtocol.
    // This is a placeholder for direct byte parsing fallback.
}
