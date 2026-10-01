#include "SerialPortWorker.h"

#include "EpuckSerComProtocol.h"
#include "MotorCommandScheduler.h"
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
    , m_healthTimer(new QTimer(this))
    , m_motorScheduler(new MotorCommandScheduler(this))
{
    m_pollTimer->setTimerType(Qt::PreciseTimer);
    connect(m_pollTimer, &QTimer::timeout, this, &SerialPortWorker::pollSensors);
    connect(m_port, &QSerialPort::readyRead, this, &SerialPortWorker::onReadyRead);
    connect(m_port, &QSerialPort::errorOccurred, this, &SerialPortWorker::onSerialError);
    m_healthTimer->setInterval(50);
    connect(m_healthTimer, &QTimer::timeout, this, &SerialPortWorker::checkTimeouts);
    connect(m_motorScheduler, &MotorCommandScheduler::commandReady, this,
            [this](const QByteArray &data, quint64, qint64) { writeData(data); });
}

SerialPortWorker::~SerialPortWorker() {
    if (m_port->isOpen()) m_port->close();
}

void SerialPortWorker::openPort() {
    if (m_closing || m_connected || m_port->isOpen()) return;

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
    if (m_closing || !m_port->isOpen()) return;
    m_port->clear(QSerialPort::Input);
    m_readBuffer.clear();
    m_handshaking = true;
    m_port->write(EpuckSerComProtocol::buildSelectorQuery());
    QTimer::singleShot(650, this, [this]() {
        if (!m_handshaking || m_closing) return;
        m_binaryHandshake = true;
        m_readBuffer.clear();
        m_port->clear(QSerialPort::Input);
        m_port->write(EpuckSerComProtocol::buildSensorPollCommand());
        QTimer::singleShot(700, this, [this]() {
            if (!m_handshaking || m_closing) return;
            m_handshaking = false;
            m_port->close();
            reportClosed("端口已打开，但未收到 e-puck 握手反馈，请检查端口和固件");
        });
    });
}

void SerialPortWorker::configureRobot() {
    m_handshaking = false;
    m_readBuffer.clear();
    m_port->write(EpuckSerComProtocol::buildLEDCommand(8, 0));
    m_port->write(EpuckSerComProtocol::buildMotorCommand(0, 0));
    m_port->write(EpuckSerComProtocol::buildCameraParams(0, 40, 40, 8));
    m_port->flush();
    QTimer::singleShot(250, this, [this]() {
        if (m_closing || !m_port->isOpen()) return;
        m_port->clear(QSerialPort::Input);
        m_readBuffer.clear();
        m_connected = true;
        m_healthTimer->start();
        emit connectionOpened(m_id);
        if (m_initialSensorData.validFields) emit sensorDataReceived(m_id, m_initialSensorData);
        startSensorPolling(m_pollIntervalMs);
    });
}

void SerialPortWorker::closePort() {
    m_closing = true;
    m_healthTimer->stop();
    m_motorScheduler->cancel();
    stopSensorPolling();
    m_connected = false;
    m_handshaking = false;
    m_awaitingSensorFrame = false;
    m_capturingImage = false;
    m_readBuffer.clear();

    if (m_port->isOpen()) {
        m_port->write(EpuckSerComProtocol::buildMotorCommand(0, 0));
        m_port->write(EpuckSerComProtocol::buildExitBinaryCommand());
        m_port->flush();
        if (m_port->bytesToWrite() > 0) m_port->waitForBytesWritten(100);
        m_port->close();
    }
    reportClosed({});
}

void SerialPortWorker::writeData(const QByteArray &data) {
    if (!m_connected || !m_port->isOpen() || data.isEmpty()) return;
    if (static_cast<uint8_t>(data[0]) == EpuckSerComProtocol::CMD_MOTOR && data.size() >= 5) {
        m_lastMotorTimer.restart();
        m_motorMoving = data.mid(1, 4) != QByteArray(4, '\0');
    }
    m_port->write(data);
}

void SerialPortWorker::scheduleMotorCommand(const QByteArray &data, qint64 deadlineMs,
                                            quint64 sequence) {
    const bool stop = data.size() >= 5 && data.mid(1, 4) == QByteArray(4, '\0');
    m_motorScheduler->schedule(data, stop ? MotorCommandScheduler::nowMs() : deadlineMs, sequence);
}

void SerialPortWorker::startSensorPolling(int intervalMs) {
    m_pollEnabled = true;
    m_pollIntervalMs = qBound(40, intervalMs, 1000);
    if (m_connected && !m_capturingImage) m_pollTimer->start(m_pollIntervalMs);
}

void SerialPortWorker::stopSensorPolling() {
    m_pollEnabled = false;
    m_pollTimer->stop();
}

void SerialPortWorker::pollSensors() {
    if (!m_connected || !m_port->isOpen() || m_capturingImage || m_recovering
        || m_awaitingSensorFrame || !m_pollEnabled) return;
    if (m_cameraQueued) { requestCameraFrame(); return; }

    m_readBuffer.clear();
    m_awaitingSensorFrame = true;
    m_sensorFrameTimer.restart();
    m_expectedSensorBytes = m_extendedPolling ? 44 : 28;
    m_port->write(m_extendedPolling ? EpuckSerComProtocol::buildTelemetryPollCommand()
                                  : EpuckSerComProtocol::buildSensorPollCommand());
}

void SerialPortWorker::requestCameraFrame() {
    if (!m_connected || !m_port->isOpen() || m_capturingImage) return;
    m_cameraQueued = true;
    if (m_awaitingSensorFrame || m_recovering) return;
    m_cameraQueued = false;

    m_pollTimer->stop();
    m_capturingImage = true;
    m_awaitingSensorFrame = false;
    m_cameraExpectedSize = 0;
    m_cameraBuffer.clear();
    m_readBuffer.clear();
    m_cameraTimer.restart();
    m_port->write(EpuckSerComProtocol::buildImageCommand());
}

void SerialPortWorker::onReadyRead() {
    const auto bytes = m_port->readAll();
    if (m_handshaking) {
        m_readBuffer.append(bytes);
        SensorData sample;
        sample.timestamp = QDateTime::currentMSecsSinceEpoch();
        const bool verified = m_binaryHandshake
            ? m_readBuffer.size() == 28 && EpuckSerComProtocol::parseSensorResponse(m_readBuffer, sample)
            : EpuckSerComProtocol::parseAsciiResponse(m_readBuffer, sample) && sample.has(SensorData::Selector);
        if (verified) { m_initialSensorData = sample; configureRobot(); }
        if (m_readBuffer.size() > 4096) m_readBuffer.clear();
        return;
    }
    if (!m_connected) return;
    if (m_recovering) { m_recoveryTimer.restart(); return; }
    m_readBuffer.append(bytes);
    processBuffer();
}

void SerialPortWorker::processBuffer() {
    if (m_capturingImage) {
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
                    recoverTransaction("摄像头帧头无效，已恢复传感器轮询");
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
                data.mark(SensorData::Camera);
                if (!data.cameraFrame.isNull()) emit sensorDataReceived(m_id, data);

                finishCamera();
                return;
            }
        }
        return;
    }

    if (m_awaitingSensorFrame) {
        if (m_readBuffer.size() < m_expectedSensorBytes) return;

        SensorData data;
        data.timestamp = QDateTime::currentMSecsSinceEpoch();
        const QByteArray frame = m_readBuffer.left(m_expectedSensorBytes);
        m_readBuffer.remove(0, m_expectedSensorBytes);
        m_awaitingSensorFrame = false;

        const bool parsed = m_extendedPolling
            ? EpuckSerComProtocol::parseTelemetryResponse(frame, data)
            : EpuckSerComProtocol::parseSensorResponse(frame, data);
        if (parsed) {
            m_sensorTimeouts = 0;
            if (!m_hasFilteredProximity) {
                m_filteredProximity = data.proximity;
                m_hasFilteredProximity = true;
            } else {
                for (int i = 0; i < 8; ++i) {
                    const int previous = m_filteredProximity[i];
                    const int current = data.proximity[i];
                    const int weight = std::abs(current - previous) > 150 ? 8 : 6;
                    m_filteredProximity[i] = static_cast<uint16_t>((previous * (10-weight) + current * weight) / 10);
                    data.proximity[i] = m_filteredProximity[i];
                }
            }
            emit sensorDataReceived(m_id, data);
        } else {
            recoverTransaction("传感器帧校验失败，正在重新同步");
        }
        if (m_cameraQueued) QTimer::singleShot(0, this, &SerialPortWorker::requestCameraFrame);
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

void SerialPortWorker::finishCamera() {
    m_capturingImage = false;
    m_cameraBuffer.clear();
    m_cameraExpectedSize = 0;
    if (m_pollEnabled) m_pollTimer->start(m_pollIntervalMs);
}

void SerialPortWorker::recoverTransaction(const QString &reason) {
    m_awaitingSensorFrame = false;
    finishCamera();
    m_recovering = true;
    m_recoveryTimer.restart();
    m_readBuffer.clear();
    m_port->clear(QSerialPort::Input);
    emit telemetryStatus(m_id, reason);
}

void SerialPortWorker::checkTimeouts() {
    if (!m_connected) return;
    if (m_motorMoving && m_lastMotorTimer.elapsed() > 1000) {
        m_motorScheduler->cancel();
        writeData(EpuckSerComProtocol::buildMotorCommand(0, 0));
        emit telemetryStatus(m_id, "控制指令超过 1 秒未更新，电机已停止");
    }
    if (m_capturingImage && m_cameraTimer.elapsed() > 2000) {
        recoverTransaction("摄像头接收超时，已恢复传感器轮询");
    } else if (m_awaitingSensorFrame && m_sensorFrameTimer.elapsed() > 500) {
        ++m_sensorTimeouts;
        if (m_extendedPolling && m_sensorTimeouts >= 2 && m_readBuffer.size() == 28) {
            m_extendedPolling = false;
            emit telemetryStatus(m_id, "固件仅响应基础传感器，已切换兼容轮询");
        }
        recoverTransaction("传感器接收超时，正在重新同步");
    }
    if (m_recovering && m_recoveryTimer.elapsed() >= 180) {
        m_recovering = false;
        if (m_pollEnabled) pollSensors();
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
