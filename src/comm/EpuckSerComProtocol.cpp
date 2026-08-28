#include "EpuckSerComProtocol.h"
#include <cstring>
#include <QRegularExpression>

// ---- Binary command builders ----

QByteArray EpuckSerComProtocol::buildSensorPollCommand() {
    QByteArray cmd;
    cmd.append(static_cast<char>(CMD_SENSOR));
    cmd.append(static_cast<char>(CMD_ACCEL));
    cmd.append(static_cast<char>(CMD_EXIT_BINARY));
    return cmd;
}

QByteArray EpuckSerComProtocol::buildMotorCommand(int16_t left, int16_t right) {
    QByteArray cmd;
    cmd.append(static_cast<char>(CMD_MOTOR));
    cmd.append(static_cast<char>(left & 0xFF));
    cmd.append(static_cast<char>((left >> 8) & 0xFF));
    cmd.append(static_cast<char>(right & 0xFF));
    cmd.append(static_cast<char>((right >> 8) & 0xFF));
    return cmd;
}

QByteArray EpuckSerComProtocol::buildLEDCommand(int ledNum, int value) {
    QByteArray cmd;
    cmd.append(static_cast<char>(CMD_LED));
    cmd.append(static_cast<char>(ledNum));
    cmd.append(static_cast<char>(value));
    return cmd;
}

QByteArray EpuckSerComProtocol::buildImageCommand() {
    QByteArray cmd;
    cmd.append(static_cast<char>(CMD_IMAGE));
    cmd.append(static_cast<char>(0x00));
    return cmd;
}

QByteArray EpuckSerComProtocol::buildRawAccelCommand() {
    QByteArray cmd;
    cmd.append(static_cast<char>(CMD_RAW_ACCEL));
    cmd.append(static_cast<char>(CMD_EXIT_BINARY));
    return cmd;
}

QByteArray EpuckSerComProtocol::buildMicCommand() {
    QByteArray cmd;
    cmd.append(static_cast<char>(CMD_MIC));
    cmd.append(static_cast<char>(CMD_EXIT_BINARY));
    return cmd;
}

QByteArray EpuckSerComProtocol::buildExitBinaryCommand() {
    return QByteArray(1, '\x00');
}

// ---- ASCII commands ----

QByteArray EpuckSerComProtocol::buildSelectorQuery() {
    return QByteArray("C\r");
}

QByteArray EpuckSerComProtocol::buildMicAsciiQuery() {
    return QByteArray("U\r");
}

QByteArray EpuckSerComProtocol::buildBodyLEDAscii(int value) {
    return QByteArray(QString("B,%1\r").arg(value).toLatin1());
}

QByteArray EpuckSerComProtocol::buildFrontLEDAscii(int value) {
    return QByteArray(QString("F,%1\r").arg(value).toLatin1());
}

QByteArray EpuckSerComProtocol::buildStopCommand() {
    return QByteArray("S\r");
}

QByteArray EpuckSerComProtocol::buildVersionQuery() {
    return QByteArray("V\r");
}

QByteArray EpuckSerComProtocol::buildResetCommand() {
    return QByteArray("R\r");
}

QByteArray EpuckSerComProtocol::buildCameraParams(int mode, int w, int h, int zoom) {
    return QByteArray(QString("J,%1,%2,%3,%4\r").arg(mode).arg(w).arg(h).arg(zoom).toLatin1());
}

QByteArray EpuckSerComProtocol::buildPingCommand() {
    return QByteArray("C\r");
}

// ---- Response parsing ----

bool EpuckSerComProtocol::parseSensorResponse(const QByteArray &data, SensorData &out) {
    const auto *d = reinterpret_cast<const uint8_t *>(data.constData());
    int len = data.size();

    // Proximity response: 8 x uint16 LE = 16 bytes (from CMD_SENSOR)
    if (len == 16) {
        for (int i = 0; i < 8; ++i) {
            out.proximity[i] = static_cast<uint16_t>(d[i * 2] | (d[i * 2 + 1] << 8));
        }
        return true;
    }

    // Accelerometer response: 3 x float LE = 12 bytes (from CMD_ACCEL)
    if (len == 12) {
        float accel, orient, inclin;
        std::memcpy(&accel, d, 4);
        std::memcpy(&orient, d + 4, 4);
        std::memcpy(&inclin, d + 8, 4);
        out.accelMagnitude = accel;
        out.accelOrientation = orient;
        out.accelInclination = inclin;
        return true;
    }

    // Combined sensor + accel response: 16 + 12 = 28 bytes
    if (len == 28) {
        // Proximity (first 16 bytes)
        for (int i = 0; i < 8; ++i) {
            out.proximity[i] = static_cast<uint16_t>(d[i * 2] | (d[i * 2 + 1] << 8));
        }
        // Accelerometer (next 12 bytes)
        std::memcpy(&out.accelMagnitude, d + 16, 4);
        std::memcpy(&out.accelOrientation, d + 20, 4);
        std::memcpy(&out.accelInclination, d + 24, 4);
        return true;
    }

    // Raw accelerometer response: 3 x int16 LE = 6 bytes (from CMD_RAW_ACCEL)
    if (len == 6) {
        int16_t ax = static_cast<int16_t>(d[0] | (d[1] << 8));
        int16_t ay = static_cast<int16_t>(d[2] | (d[3] << 8));
        int16_t az = static_cast<int16_t>(d[4] | (d[5] << 8));
        out.accelerometer.x = ax;
        out.accelerometer.y = ay;
        out.accelerometer.z = az;
        return true;
    }

    // Binary microphone response: 3 x uint16 LE = 6 bytes (from CMD_MIC)
    if (len == 6) {
        for (int i = 0; i < 3; ++i) {
            out.rawMicrophones[i] = static_cast<uint16_t>(d[i * 2] | (d[i * 2 + 1] << 8));
        }
        return true;
    }

    return false;
}

QImage EpuckSerComProtocol::parseImageResponse(const QByteArray &data) {
    if (data.size() < 3) return {};

    const auto *d = reinterpret_cast<const uint8_t *>(data.constData());
    uint8_t type = d[0];
    uint8_t w = d[1];
    uint8_t h = d[2];

    int headerSize = 3;
    int expectedPixels = (type == 0) ? (w * h) : (w * h * 2);

    if (data.size() < headerSize + expectedPixels) return {};

    const uint8_t *pixels = d + headerSize;

    if (type == 0) {
        return parseGreyscaleImage(pixels, w, h);
    } else if (type == 1) {
        return parseRGB565Image(pixels, w, h);
    }

    return {};
}

QImage EpuckSerComProtocol::parseGreyscaleImage(const uint8_t *pixels, int w, int h) {
    QImage img(w, h, QImage::Format_Grayscale8);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // Apply -90 degree rotation (official monitor convention)
            int srcX = y;
            int srcY = w - 1 - x;
            if (srcX >= 0 && srcX < w && srcY >= 0 && srcY < h) {
                img.setPixel(x, y, pixels[srcY * w + srcX]);
            }
        }
    }
    return img;
}

QImage EpuckSerComProtocol::parseRGB565Image(const uint8_t *pixels, int w, int h) {
    QImage img(w, h, QImage::Format_RGB32);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int srcX = y;
            int srcY = w - 1 - x;
            if (srcX >= 0 && srcX < w && srcY >= 0 && srcY < h) {
                int offset = (srcY * w + srcX) * 2;
                uint8_t hi = pixels[offset];
                uint8_t lo = pixels[offset + 1];
                int r = hi & 0xF8;
                int g = ((hi & 0x07) << 5) | ((lo & 0xE0) >> 3);
                int b = (lo & 0x1F) << 3;
                img.setPixel(x, y, qRgb(r, g, b));
            }
        }
    }
    return img;
}

bool EpuckSerComProtocol::parseAsciiResponse(const QByteArray &line, SensorData &out) {
    if (line.isEmpty()) return false;

    // Selector: "c,N\r\n"
    if (line.startsWith("c,")) {
        bool ok;
        int val = line.mid(2).trimmed().toInt(&ok);
        if (ok) {
            out.selectorPosition = val;
            return true;
        }
    }

    // Microphone ASCII: "u,v1,v2,v3\r\n"
    if (line.startsWith("u,")) {
        QList<QByteArray> parts = line.mid(2).trimmed().split(',');
        if (parts.size() >= 3) {
            for (int i = 0; i < 3 && i < 3; ++i) {
                out.rawMicrophones[i] = static_cast<uint16_t>(parts[i].toInt());
            }
            return true;
        }
    }

    // IR check: "g IR check : 0xHH, address : 0xHH, data : 0xHH"
    if (line.startsWith("g ")) {
        QString str = QString::fromLatin1(line);
        QRegularExpression rx("check\\s*:\\s*0x([0-9a-fA-F]+).*address\\s*:\\s*0x([0-9a-fA-F]+).*data\\s*:\\s*0x([0-9a-fA-F]+)");
        auto match = rx.match(str);
        if (match.hasMatch()) {
            out.irCheck = static_cast<uint8_t>(match.captured(1).toInt(nullptr, 16));
            out.irAddress = static_cast<uint8_t>(match.captured(2).toInt(nullptr, 16));
            out.irData = static_cast<uint8_t>(match.captured(3).toInt(nullptr, 16));
            return true;
        }
    }

    // Version: "v,Version ..."
    if (line.startsWith("v,")) {
        return true; // acknowledged, no data to extract
    }

    return false;
}

bool EpuckSerComProtocol::isSelectorResponse(const QByteArray &data) {
    const QByteArray trimmed = data.trimmed().toLower();
    return trimmed.startsWith("c,") || trimmed.startsWith("c ");
}
