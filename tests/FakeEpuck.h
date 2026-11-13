#pragma once

// A stand-in e-puck for link tests: a pseudo terminal that speaks enough of the
// SerCom protocol (selector handshake, binary telemetry, motor and LED commands)
// for the real SerialPortWorker to connect to it. POSIX only; the tests that use
// it are compiled out on Windows.

#include <QtGlobal>
#ifdef Q_OS_UNIX

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPair>
#include <QSocketNotifier>
#include <QString>

#include <cstring>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

class FakeEpuck : public QObject {
    Q_OBJECT
public:
    explicit FakeEpuck(QObject *parent = nullptr) : QObject(parent) {
        m_master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (m_master < 0 || grantpt(m_master) != 0 || unlockpt(m_master) != 0) return;
        m_path = QString::fromLocal8Bit(ptsname(m_master));
        // Holding the slave open keeps the master readable while the application
        // closes and reopens its side, as a paired Bluetooth port would stay present.
        m_keepAlive = ::open(m_path.toLocal8Bit().constData(), O_RDWR | O_NOCTTY);
        m_notifier = new QSocketNotifier(m_master, QSocketNotifier::Read, this);
        connect(m_notifier, &QSocketNotifier::activated, this, &FakeEpuck::onReadable);
    }

    ~FakeEpuck() override { unplug(); }

    bool isValid() const { return m_master >= 0 && !m_path.isEmpty(); }
    QString portPath() const { return m_path; }

    // Stop answering anything: a link that is open but dead.
    void setMuted(bool muted) { m_muted = muted; }
    // Answer the handshake but no telemetry: firmware without the binary sensors.
    void setTelemetryEnabled(bool enabled) { m_telemetry = enabled; }
    // Wheel speed fed back through the encoders, in steps per telemetry poll.
    void setEncoderStep(int left, int right) { m_stepLeft = left; m_stepRight = right; }

    // The device disappears: both ends of the pseudo terminal are closed.
    void unplug() {
        if (m_notifier) { m_notifier->setEnabled(false); delete m_notifier; m_notifier = nullptr; }
        if (m_keepAlive >= 0) { ::close(m_keepAlive); m_keepAlive = -1; }
        if (m_master >= 0) { ::close(m_master); m_master = -1; }
    }

    const QList<QPair<int, int>> &motorCommands() const { return m_motorCommands; }
    QPair<int, int> lastMotorCommand() const {
        return m_motorCommands.isEmpty() ? QPair<int, int>{0, 0} : m_motorCommands.last();
    }
    void clearMotorCommands() { m_motorCommands.clear(); }
    int telemetryPolls() const { return m_polls; }
    int handshakes() const { return m_handshakes; }
    int ledCommands() const { return m_ledCommands; }

signals:
    void motorCommandReceived(int left, int right);

private slots:
    void onReadable() {
        char buffer[512];
        for (;;) {
            const ssize_t count = ::read(m_master, buffer, sizeof(buffer));
            if (count <= 0) break;
            m_input.append(buffer, static_cast<int>(count));
        }
        process();
    }

private:
    static void appendWord(QByteArray &out, int value) {
        out.append(static_cast<char>(value & 0xFF));
        out.append(static_cast<char>((value >> 8) & 0xFF));
    }

    void reply(const QByteArray &data) {
        if (m_muted || m_master < 0 || data.isEmpty()) return;
        const ssize_t written = ::write(m_master, data.constData(), static_cast<size_t>(data.size()));
        Q_UNUSED(written);
    }

    // BTcom framing: a first byte with the high bit set starts a binary group that
    // runs until a zero command byte; anything else is an ASCII line ending in CR.
    void process() {
        for (;;) {
            if (m_input.isEmpty()) return;
            const auto first = static_cast<quint8>(m_input[0]);
            if (!m_binary && first == 0x00) { m_input.remove(0, 1); continue; }  // stray terminator
            if (!m_binary && first < 0x80) {
                const int end = m_input.indexOf('\r');
                if (end < 0) { if (m_input.size() > 256) m_input.clear(); return; }
                const QByteArray line = m_input.left(end).trimmed();
                m_input.remove(0, end + 1);
                if (line.startsWith("C")) { ++m_handshakes; reply("c,5\r\n"); }
                continue;
            }
            m_binary = true;
            if (first == 0x00) {            // end of binary group
                m_input.remove(0, 1);
                m_binary = false;
                reply(m_pending);
                m_pending.clear();
                continue;
            }
            if (first == 0xBC) {            // motor: 4 parameter bytes
                if (m_input.size() < 5) return;
                const auto *d = reinterpret_cast<const quint8 *>(m_input.constData());
                const int left = static_cast<qint16>(d[1] | (d[2] << 8));
                const int right = static_cast<qint16>(d[3] | (d[4] << 8));
                m_input.remove(0, 5);
                m_motorCommands.append({left, right});
                emit motorCommandReceived(left, right);
                continue;
            }
            if (first == 0xB4) {            // LED: 2 parameter bytes
                if (m_input.size() < 3) return;
                m_input.remove(0, 3);
                ++m_ledCommands;
                continue;
            }
            m_input.remove(0, 1);
            if (first == 0xB2) ++m_polls;   // counted even when unanswered
            if (!m_telemetry) continue;
            switch (first) {
            case 0xB2:                      // 8 proximity words
                for (int i = 0; i < 8; ++i) appendWord(m_pending, 100 + i * 10);
                break;
            case 0xBF: {                    // spherical acceleration, 3 floats
                const float values[3] = {1000.0f, 90.0f, 5.0f};
                m_pending.append(reinterpret_cast<const char *>(values), sizeof(values));
                break;
            }
            case 0x9F:                      // raw acceleration, 3 words
                appendWord(m_pending, 10); appendWord(m_pending, -20); appendWord(m_pending, 800);
                break;
            case 0xAF:                      // encoders, 2 words
                m_encoderLeft = static_cast<qint16>(m_encoderLeft + m_stepLeft);
                m_encoderRight = static_cast<qint16>(m_encoderRight + m_stepRight);
                appendWord(m_pending, m_encoderLeft); appendWord(m_pending, m_encoderRight);
                break;
            case 0x8B:                      // microphones, 3 words
                appendWord(m_pending, 300); appendWord(m_pending, 310); appendWord(m_pending, 320);
                break;
            default:
                break;
            }
        }
    }

    int m_master = -1;
    int m_keepAlive = -1;
    QSocketNotifier *m_notifier = nullptr;
    QString m_path;
    QByteArray m_input;
    QByteArray m_pending;
    bool m_binary = false;
    bool m_muted = false;
    bool m_telemetry = true;
    int m_stepLeft = 0, m_stepRight = 0;
    qint16 m_encoderLeft = 0, m_encoderRight = 0;
    int m_polls = 0, m_handshakes = 0, m_ledCommands = 0;
    QList<QPair<int, int>> m_motorCommands;
};

#endif // Q_OS_UNIX
