#include "DFUProgrammer.h"
#include "util/Logger.h"

DFUProgrammer::DFUProgrammer(QObject *parent)
    : QObject(parent)
{
}

DFUProgrammer::~DFUProgrammer() {
    closeDevice();
}

bool DFUProgrammer::openDevice(uint16_t vid, uint16_t pid) {
#ifdef HAS_LIBUSB
    if (m_deviceHandle) closeDevice();

    int rc = libusb_init(&m_usbContext);
    if (rc < 0) {
        emit errorOccurred(QString("libusb_init failed: %1").arg(rc));
        return false;
    }

    m_deviceHandle = libusb_open_device_with_vid_pid(m_usbContext, vid, pid);
    if (!m_deviceHandle) {
        emit errorOccurred("STM32 DFU device not found. Ensure robot is in DFU mode.");
        return false;
    }

    libusb_set_auto_detach_kernel_driver(m_deviceHandle, 1);
    m_open = true;
    qCDebug(logDFU) << "DFU device opened:" << Qt::hex << vid << pid;
    return true;
#else
    Q_UNUSED(vid) Q_UNUSED(pid)
    emit errorOccurred("libusb support not compiled. Install libusb and rebuild.");
    return false;
#endif
}

void DFUProgrammer::closeDevice() {
#ifdef HAS_LIBUSB
    if (m_deviceHandle) {
        libusb_close(m_deviceHandle);
        m_deviceHandle = nullptr;
    }
    if (m_usbContext) {
        libusb_exit(m_usbContext);
        m_usbContext = nullptr;
    }
#endif
    m_open = false;
}

bool DFUProgrammer::isOpen() const {
    return m_open;
}

void DFUProgrammer::flashFirmware(const QString &filePath, uint32_t startAddress) {
    // TODO: Implement DFU flashing in Phase 6
    // For now, emit progress and return
    Q_UNUSED(startAddress)
    qCDebug(logDFU) << "Flash firmware stub called for:" << filePath;
    emit errorOccurred("DFU flashing not yet implemented. Phase 6.");
    emit flashFinished(false);
}
