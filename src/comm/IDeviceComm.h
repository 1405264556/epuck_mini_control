#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

class IDeviceComm {
public:
    virtual ~IDeviceComm() = default;
    virtual bool isConnected() const = 0;
    virtual void connectToDevice() = 0;
    virtual void disconnectFromDevice() = 0;
    virtual bool writeData(const QByteArray &data) = 0;
};
