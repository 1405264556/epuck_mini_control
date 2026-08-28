#pragma once

#include <QObject>
#include <QString>

#ifdef HAS_LIBUSB
#include <libusb-1.0/libusb.h>
#endif

class DFUProgrammer : public QObject {
    Q_OBJECT
public:
    explicit DFUProgrammer(QObject *parent = nullptr);
    ~DFUProgrammer() override;

    bool openDevice(uint16_t vid = 0x0483, uint16_t pid = 0xDF11);
    void closeDevice();
    bool isOpen() const;

    // High-level firmware flash
    void flashFirmware(const QString &filePath, uint32_t startAddress = 0x08000000);

signals:
    void flashProgress(int percent);
    void flashFinished(bool success);
    void errorOccurred(const QString &msg);

private:
#ifdef HAS_LIBUSB
    libusb_context *m_usbContext = nullptr;
    libusb_device_handle *m_deviceHandle = nullptr;
#endif
    bool m_open = false;
    uint16_t m_transferSize = 2048;
};
