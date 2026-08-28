#pragma once

#include <QBluetoothAddress>
#include <QBluetoothDeviceInfo>
#include <QString>
#include "core/Types.h"

struct BLEDeviceInfo {
    QBluetoothDeviceInfo qtDeviceInfo;
    QString friendlyName;
    QString portName;        // COM port name (for serial devices)
    QString bluetoothAddr;   // Bluetooth MAC (for BLE devices)
    int rssi = 0;
    int baudRate = 115200;    // Serial baud rate confirmed during scan
    bool isEpuckMini = false;

    RobotId deriveId() const {
        if (!portName.isEmpty())
            return QString("COM:%1").arg(portName);
        if (!bluetoothAddr.isEmpty())
            return bluetoothAddr;
        return {};
    }
};
