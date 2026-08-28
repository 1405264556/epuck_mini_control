#include "SerialPortWorker.h"

#include "EpuckSerComProtocol.h"
#include "util/Logger.h"

#include <QDateTime>

namespace {
void configurePort(QSerialPort *port, const QString &portName, int baudRate) {
    port->setPortName(portName);
    port->setBaudRate(baudRate);
    port->setDataBits(QSerialPort::Data8);
    port->setParity(QSerialPort::NoParity);
    port->setStopBits(QSerialPort::OneStop);
    port->setFlowControl(QSerialPort::NoFlowControl);
}
}

SerialPortWorker::SerialPortWorker(const RobotId &id, const QString &portName,
                                   int baudRate, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_portName(portName)
    , m_baudRate(baudRate)
    , m_port(new QSerialPort(this))
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setTimerType(Qt::PreciseTimer);
    connect(m_pollTimer, &QTimer::timeout, this, &SerialPortWorker::pollSensors);
    connect(m_port, &QSerialPort::readyRead, this, &SerialPortWorker::onReadyRead);
    connect(m_port, &QSerialPort::errorOccurred, this, &SerialPortWorker::onSerialError);
}

SerialPortWorker::~SerialPortWorker() {
    if (m_port->isOpen()) m_port->close();
}

void SerialPortWorker::openPort() {
    if (m_connected || m_port->isOpen()) return;

    configurePort(m_port, m_portName, m_baudRate);
    ++m_openAttempts;
    if (!m_port->open(QIODevice::ReadWrite)) {
        const QString reason = m_port->errorString();
        if (m_openAttempts < 8) {
            QTimer::singleShot(120, this, &SerialPortWorker::openPort);
            return;
        }
        reportClosed(QString("无法打开 %1：%2").arg(m_portName, reason));
        return;
    }

    m_port->clear();
    m_port->write("\r");
    QTimer::singleShot(150, this, &SerialPortWorker::initializeRobot);
}

void SerialPortWorker::initializeRobot() {
    if (!m_port->isOpen()) return;

    m_port->clear(QSerialPort::Input);
    m_port->write(EpuckSerComProtocol::buildLEDCommand(8, 0));
    m_port->write(EpuckSerComProtocol::buildExitBinaryCommand());
    m_port->write("T,0\r");
    m_port->write("B,0\r");
    m_port->write("F,0\r");
    m_port->flush();
    m_port->clear(QSerialPort::Input);

    m_connected = true;
    emit connectionOpened(m_id);
    QTimer::singleShot(180, this, [this]() { startSensorPolling(m_pollIntervalMs); });
}

void SerialPortWorker::closePort() {
    stopSensorPolling();
    m_connected = false;
    m_awaitingSensorFrame = false;
    m_capturingImage = false;
    m_readBuffer.clear();

    if (m_port->isOpen()) {
        m_port->write(EpuckSerComProtocol::buildMotorCommand(0, 0));
        m_port->write(EpuckSerComProtocol::buildExitBinaryCommand());
        m_port->flush();
        m_port->close();
    }
    reportClosed({});
}

void SerialPortWorker::writeData(const QByteArray &data) {
    if (!m_connected || !m_port->isOpen() || data.isEmpty()) return;
    m_port->write(data);
}

void SerialPortWorker::startSensorPolling(int intervalMs) {
    m_pollIntervalMs = qBound(45, intervalMs, 1000);
    if (m_connected && !m_capturingImage) m_pollTimer->start(m_pollIntervalMs);
}

void SerialPortWorker::stopSensorPolling() {
    m_pollTimer->stop();
}

void SerialPortWorker::pollSensors() {
    if (!m_connected || !m_port->isOpen() || m_capturingImage) return;
    if (m_awaitingSensorFrame) {
        if (!m_sensorFrameTimer.isValid() || m_sensorFrameTimer.elapsed() <= 140) return;
        m_awaitingSensorFrame = false;
        m_readBuffer.clear();
    }

    m_readBuffer.clear();
    m_awaitingSensorFrame = true;
    m_sensorFrameTimer.restart();
    m_port->clear(QSerialPort::Input);
    m_port->write(EpuckSerComProtocol::buildSensorPollCommand());
}

void SerialPortWorker::requestCameraFrame() {
    if (!m_connected || !m_port->isOpen() || m_capturingImage) return;

    m_pollTimer->stop();
    m_capturingImage = true;
    m_awaitingSensorFrame = false;
    m_cameraExpectedSize = 0;
    m_cameraBuffer.clear();
    m_readBuffer.clear();
    m_port->clear(QSerialPort::Input);
    m_port->write(EpuckSerComProtocol::buildImageCommand());
}

void SerialPortWorker::onReadyRead() {
    m_readBuffer.append(m_port->readAll());
    processBuffer();
}

void SerialPortWorker::processBuffer() {
    if (m_capturingImage) {
        const QByteArray imageCommand = EpuckSerComProtocol::buildImageCommand();
        if (m_cameraExpectedSize == 0 && m_readBuffer.startsWith(imageCommand)) {
            m_readBuffer.remove(0, imageCommand.size());
        }

        while (!m_readBuffer.isEmpty()) {
            if (m_cameraExpectedSize == 0) {
                if (m_readBuffer.size() < 3) return;
                m_cameraBuffer = m_readBuffer.left(3);
                m_readBuffer.remove(0, 3);

                const auto *header = reinterpret_cast<const uint8_t *>(m_cameraBuffer.constData());
                const int imageType = header[0];
                const int width = header[1];
                const int height = header[2];
                if (width <= 0 || height <= 0 || width > 160 || height > 160
                    || (imageType != 0 && imageType != 1)) {
                    m_capturingImage = false;
                    m_cameraBuffer.clear();
                    m_cameraExpectedSize = 0;
                    m_pollTimer->start(m_pollIntervalMs);
                    return;
                }
                m_cameraExpectedSize = 3 + (imageType == 0 ? width * height : width * height * 2);
            }

            const int needed = m_cameraExpectedSize - m_cameraBuffer.size();
            const int take = qMin(needed, m_readBuffer.size());
            m_cameraBuffer.append(m_readBuffer.left(take));
            m_readBuffer.remove(0, take);

            if (m_cameraBuffer.size() >= m_cameraExpectedSize) {
                SensorData data;
                data.timestamp = QDateTime::currentMSecsSinceEpoch();
                data.cameraFrame = EpuckSerComProtocol::parseImageResponse(m_cameraBuffer);
                if (!data.cameraFrame.isNull()) emit sensorDataReceived(m_id, data);

                m_capturingImage = false;
                m_cameraBuffer.clear();
                m_cameraExpectedSize = 0;
                m_pollTimer->start(m_pollIntervalMs);
                return;
            }
        }
        return;
    }

    if (m_awaitingSensorFrame) {
        const QByteArray command = EpuckSerComProtocol::buildSensorPollCommand();
        if (m_readBuffer.startsWith(command)) m_readBuffer.remove(0, command.size());
        if (m_readBuffer.size() < 28) return;

        SensorData data;
        data.timestamp = QDateTime::currentMSecsSinceEpoch();
        const QByteArray frame = m_readBuffer.left(28);
        m_readBuffer.remove(0, 28);
        m_awaitingSensorFrame = false;

        if (EpuckSerComProtocol::parseSensorResponse(frame, data)) {
            if (!m_hasFilteredProximity) {
                m_filteredProximity = data.proximity;
                m_hasFilteredProximity = true;
            } else {
                for (int i = 0; i < 8; ++i) {
                    const int previous = m_filteredProximity[i];
                    const int current = data.proximity[i];
                    m_filteredProximity[i] = static_cast<uint16_t>((previous * 2 + current) / 3);
                    data.proximity[i] = m_filteredProximity[i];
                }
            }
            emit sensorDataReceived(m_id, data);
        }
    }

    while (m_readBuffer.contains('\n')) {
        const int newline = m_readBuffer.indexOf('\n');
        const QByteArray line = m_readBuffer.left(newline + 1);
        m_readBuffer.remove(0, newline + 1);

        SensorData data;
        data.timestamp = QDateTime::currentMSecsSinceEpoch();
        if (EpuckSerComProtocol::parseAsciiResponse(line.trimmed(), data)) {
            emit sensorDataReceived(m_id, data);
        }
    }

    if (m_readBuffer.size() > 4096) {
        qCWarning(logSerial) << "Serial receive buffer reset for" << m_id;
        m_readBuffer.clear();
    }
}

void SerialPortWorker::onSerialError(QSerialPort::SerialPortError error) {
    if (error == QSerialPort::NoError || !m_connected) return;
    if (error != QSerialPort::ResourceError
        && error != QSerialPort::DeviceNotFoundError
        && error != QSerialPort::PermissionError) {
        return;
    }

    const QString reason = QString("串口 %1 异常：%2").arg(m_portName, m_port->errorString());
    stopSensorPolling();
    m_connected = false;
    if (m_port->isOpen()) m_port->close();
    reportClosed(reason);
}

void SerialPortWorker::reportClosed(const QString &reason) {
    if (m_closedReported) return;
    m_closedReported = true;
    emit connectionClosed(m_id, reason);
}
