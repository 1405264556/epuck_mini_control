#include "EpuckSerComProtocol.h"
#include <cstring>
#include <QRegularExpression>
#include <QTransform>
#include <cmath>

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
    cmd.append('\0');
    return cmd;
}

QByteArray EpuckSerComProtocol::buildLEDCommand(int ledNum, int value) {
    QByteArray cmd;
    cmd.append(static_cast<char>(CMD_LED));
    cmd.append(static_cast<char>(ledNum));
    cmd.append(static_cast<char>(value));
    cmd.append('\0');
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

QByteArray EpuckSerComProtocol::buildTelemetryPollCommand() {
    QByteArray command = buildSensorPollCommand();
    command.chop(1);
    command.append(static_cast<char>(CMD_RAW_ACCEL));
    command.append(static_cast<char>(CMD_ENCODERS));
    command.append(static_cast<char>(CMD_MIC));
    command.append('\0');
    return command;
}

bool EpuckSerComProtocol::parseTelemetryResponse(const QByteArray &data, SensorData &out) {
    if (data.size() != 44) return false;
    SensorData parsed;
    parsed.timestamp = out.timestamp;
    if (!parseSensorResponse(data.left(28), parsed)
        || !parseBinaryResponse(CMD_RAW_ACCEL, data.mid(28, 6), parsed)
        || !parseBinaryResponse(CMD_ENCODERS, data.mid(34, 4), parsed)
        || !parseBinaryResponse(CMD_MIC, data.mid(38, 6), parsed)) return false;
    out.merge(parsed);
    return true;
}

bool EpuckSerComProtocol::parseBinaryResponse(uint8_t command, const QByteArray &data,
                                             SensorData &out) {
    if (command == CMD_SENSOR && data.size() == 16) return parseSensorResponse(data, out);
    if (command == CMD_ACCEL && data.size() == 12) return parseSensorResponse(data, out);
    const auto *d = reinterpret_cast<const uint8_t *>(data.constData());
    auto signedWord = [d](int offset) { return static_cast<int16_t>(d[offset] | (d[offset + 1] << 8)); };
    if (command == CMD_RAW_ACCEL && data.size() == 6) {
        out.accelerometer = {static_cast<float>(signedWord(0)),
                             static_cast<float>(signedWord(2)),
                             static_cast<float>(signedWord(4))};
        out.accelerometerInADC = true;
        out.mark(SensorData::Accelerometer);
        return true;
    }
    if (command == CMD_ENCODERS && data.size() == 4) {
        out.encoders = {signedWord(0), signedWord(2)};
        out.mark(SensorData::Encoders);
        return true;
    }
    if (command == CMD_MIC && data.size() == 6) {
        out.microphoneChannels = 3;
        for (int i = 0; i < 3; ++i) {
            out.rawMicrophones[i] = static_cast<uint16_t>(d[2*i] | (d[2*i+1] << 8));
            out.microphones[i] = qBound(0.0f, out.rawMicrophones[i] / 4095.0f, 1.0f);
        }
        out.mark(SensorData::Microphone);
        return true;
    }
    return false;
}

bool EpuckSerComProtocol::parseSensorResponse(const QByteArray &data, SensorData &out) {
    const auto *d = reinterpret_cast<const uint8_t *>(data.constData());
    int len = data.size();

    // Proximity response: 8 x uint16 LE = 16 bytes (from CMD_SENSOR)
    if (len == 16) {
        for (int i = 0; i < 8; ++i) {
            const auto raw = static_cast<int16_t>(d[i * 2] | (d[i * 2 + 1] << 8));
            out.proximity[i] = static_cast<uint16_t>(qBound(0, static_cast<int>(raw), 4095));
        }
        out.mark(SensorData::Proximity);
        return true;
    }

    // Accelerometer response: 3 x float LE = 12 bytes (from CMD_ACCEL)
    if (len == 12) {
        float accel, orient, inclin;
        std::memcpy(&accel, d, 4);
        std::memcpy(&orient, d + 4, 4);
        std::memcpy(&inclin, d + 8, 4);
        if (!std::isfinite(accel) || !std::isfinite(orient) || !std::isfinite(inclin)) return false;
        out.accelMagnitude = accel;
        out.accelOrientation = orient;
        out.accelInclination = inclin;
        out.mark(SensorData::AccelSpherical);
        return true;
    }

    // Combined sensor + accel response: 16 + 12 = 28 bytes
    if (len == 28) {
        SensorData parsed;
        parsed.timestamp = out.timestamp;
        if (!parseSensorResponse(data.left(16), parsed)
            || !parseSensorResponse(data.mid(16, 12), parsed)) return false;
        out.merge(parsed);
        return true;
    }

    // Raw accelerometer response: 3 x int16 LE = 6 bytes (from CMD_RAW_ACCEL)
    if (len == 6) {
        return parseBinaryResponse(CMD_RAW_ACCEL, data, out);
    }

    return false;
}

QImage EpuckSerComProtocol::parseImageResponse(const QByteArray &data) {
    if (data.size() < 3) return {};

    const auto *d = reinterpret_cast<const uint8_t *>(data.constData());
    uint8_t type = d[0];
    uint8_t w = d[1];
    uint8_t h = d[2];
    if (type > 1 || w == 0 || h == 0 || w > 160 || h > 160) return {};

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
            img.scanLine(y)[x] = pixels[y * w + x];
        }
    }
    return img.transformed(QTransform().rotate(-90));
}

QImage EpuckSerComProtocol::parseRGB565Image(const uint8_t *pixels, int w, int h) {
    QImage img(w, h, QImage::Format_RGB32);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
                int offset = (y * w + x) * 2;
                uint8_t hi = pixels[offset];
                uint8_t lo = pixels[offset + 1];
                int r = hi & 0xF8;
                int g = ((hi & 0x07) << 5) | ((lo & 0xE0) >> 3);
                int b = (lo & 0x1F) << 3;
                img.setPixel(x, y, qRgb(r, g, b));
        }
    }
    return img.transformed(QTransform().rotate(-90));
}

bool EpuckSerComProtocol::parseAsciiResponse(const QByteArray &line, SensorData &out) {
    if (line.isEmpty()) return false;

    // Selector: "c,N\r\n"
    static const QRegularExpression selector("(?:^|[\\r\\n])c[, ]\\s*(\\d{1,2})(?:[\\r\\n]|$)", QRegularExpression::CaseInsensitiveOption);
    const auto selectorMatch = selector.match(QString::fromLatin1(line));
    if (selectorMatch.hasMatch() && selectorMatch.captured(1).toInt() <= 15) {
        out.selectorPosition = selectorMatch.captured(1).toInt();
        out.mark(SensorData::Selector);
        return true;
    }

    // Microphone ASCII: "u,v1,v2,v3\r\n"
    if (line.startsWith("u,")) {
        QList<QByteArray> parts = line.mid(2).trimmed().split(',');
        if (parts.size() >= 3) {
            for (int i = 0; i < 3; ++i) {
                bool ok = false;
                const int value = parts[i].toInt(&ok);
                if (!ok || value < 0 || value > 65535) return false;
                out.rawMicrophones[i] = static_cast<uint16_t>(value);
                out.microphones[i] = qBound(0.0f, value / 4095.0f, 1.0f);
            }
            out.mark(SensorData::Microphone);
            out.microphoneChannels = 3;
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
            out.mark(SensorData::Infrared);
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
    SensorData sample;
    return parseAsciiResponse(data, sample) && sample.has(SensorData::Selector);
}
