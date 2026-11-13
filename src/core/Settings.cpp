#include "Settings.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
    constexpr int DEFAULT_BLE_SCAN_TIMEOUT_MS = 10000;
    constexpr int DEFAULT_BLE_RECONNECT_ATTEMPTS = 3;
    constexpr int DEFAULT_BLE_RSSI_THRESHOLD = -80;
    constexpr int DEFAULT_DISPLAY_REFRESH_HZ = 30;
    constexpr int DEFAULT_CHART_HISTORY_SECS = 10;
    constexpr double DEFAULT_MOTOR_SPEED = 0.5;
    constexpr double DEFAULT_JOYSTICK_SENSITIVITY = 1.0;
    constexpr int DEFAULT_CONTROL_LOOP_HZ = 20;
}

Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_settings("EPFL", "e-puckMiniControl")
{
}

Settings::Settings(const QString &iniFilePath, QObject *parent)
    : QObject(parent)
    , m_settings(iniFilePath, QSettings::IniFormat)
{
}

// BLE
int Settings::bleScanTimeoutMs() const {
    return m_settings.value("ble/scanTimeoutMs", DEFAULT_BLE_SCAN_TIMEOUT_MS).toInt();
}
void Settings::setBleScanTimeoutMs(int ms) { m_settings.setValue("ble/scanTimeoutMs", ms); }

int Settings::bleReconnectAttempts() const {
    return m_settings.value("ble/reconnectAttempts", DEFAULT_BLE_RECONNECT_ATTEMPTS).toInt();
}
void Settings::setBleReconnectAttempts(int n) { m_settings.setValue("ble/reconnectAttempts", n); }

int Settings::bleRssiThreshold() const {
    return m_settings.value("ble/rssiThreshold", DEFAULT_BLE_RSSI_THRESHOLD).toInt();
}
void Settings::setBleRssiThreshold(int dbm) { m_settings.setValue("ble/rssiThreshold", dbm); }

// Display
int Settings::displayRefreshRateHz() const {
    return m_settings.value("display/refreshRateHz", DEFAULT_DISPLAY_REFRESH_HZ).toInt();
}
void Settings::setDisplayRefreshRateHz(int hz) { m_settings.setValue("display/refreshRateHz", hz); }

int Settings::chartHistorySeconds() const {
    return m_settings.value("display/chartHistorySeconds", DEFAULT_CHART_HISTORY_SECS).toInt();
}
void Settings::setChartHistorySeconds(int s) { m_settings.setValue("display/chartHistorySeconds", s); }

// Control
double Settings::defaultMotorSpeed() const {
    return m_settings.value("control/defaultSpeed", DEFAULT_MOTOR_SPEED).toDouble();
}
void Settings::setDefaultMotorSpeed(double s) { m_settings.setValue("control/defaultSpeed", s); }

double Settings::joystickSensitivity() const {
    return m_settings.value("control/joystickSensitivity", DEFAULT_JOYSTICK_SENSITIVITY).toDouble();
}
void Settings::setJoystickSensitivity(double s) { m_settings.setValue("control/joystickSensitivity", s); }

int Settings::controlLoopRateHz() const {
    return m_settings.value("control/controlLoopHz", DEFAULT_CONTROL_LOOP_HZ).toInt();
}
void Settings::setControlLoopRateHz(int hz) { m_settings.setValue("control/controlLoopHz", hz); }

// Logging
int Settings::logLevel() const {
    return m_settings.value("logging/level", 0).toInt();
}
void Settings::setLogLevel(int level) { m_settings.setValue("logging/level", level); }

// Window state
QByteArray Settings::mainWindowGeometry() const {
    return m_settings.value("geometry/mainWindow").toByteArray();
}
void Settings::setMainWindowGeometry(const QByteArray &geo) {
    m_settings.setValue("geometry/mainWindow", geo);
}

QByteArray Settings::mainWindowState() const {
    return m_settings.value("geometry/dockState").toByteArray();
}
void Settings::setMainWindowState(const QByteArray &state) {
    m_settings.setValue("geometry/dockState", state);
}

// General
QString Settings::lastFirmwareDirectory() const {
    return m_settings.value("general/lastFwDir", "").toString();
}
void Settings::setLastFirmwareDirectory(const QString &dir) {
    m_settings.setValue("general/lastFwDir", dir);
}

// Connection
QList<Settings::KnownDevice> Settings::knownDevices() const {
    // Kept as one JSON string so the value reads the same in the registry and in INI files.
    QList<KnownDevice> devices;
    const auto document = QJsonDocument::fromJson(
        m_settings.value("connection/knownDevices").toString().toUtf8());
    const QJsonArray stored = document.array();
    for (const auto &entry : stored) {
        const QJsonObject object = entry.toObject();
        KnownDevice device;
        device.portName = object.value("port").toString();
        device.name = object.value("name").toString();
        device.baudRate = object.value("baud").toInt(115200);
        device.wasConnected = object.value("connected").toBool(false);
        if (!device.portName.isEmpty()) devices.append(device);
    }
    return devices;
}

void Settings::setKnownDevices(const QList<KnownDevice> &devices) {
    QJsonArray stored;
    for (const auto &device : devices) {
        if (device.portName.isEmpty()) continue;
        stored.append(QJsonObject{{"port", device.portName}, {"name", device.name},
                                  {"baud", device.baudRate},
                                  {"connected", device.wasConnected}});
    }
    m_settings.setValue("connection/knownDevices",
                        QString::fromUtf8(QJsonDocument(stored).toJson(QJsonDocument::Compact)));
}

bool Settings::autoReconnect() const {
    return m_settings.value("connection/autoReconnect", true).toBool();
}
void Settings::setAutoReconnect(bool enabled) { m_settings.setValue("connection/autoReconnect", enabled); }

int Settings::reconnectAttempts() const {
    return qBound(1, m_settings.value("connection/reconnectAttempts", 3).toInt(), 10);
}
void Settings::setReconnectAttempts(int attempts) {
    m_settings.setValue("connection/reconnectAttempts", qBound(1, attempts, 10));
}

bool Settings::autoConnectOnStartup() const {
    return m_settings.value("connection/autoConnectOnStartup", false).toBool();
}
void Settings::setAutoConnectOnStartup(bool enabled) {
    m_settings.setValue("connection/autoConnectOnStartup", enabled);
}

int Settings::sensorPollIntervalMs() const {
    return qBound(40, m_settings.value("connection/sensorPollIntervalMs", 60).toInt(), 1000);
}
void Settings::setSensorPollIntervalMs(int ms) {
    m_settings.setValue("connection/sensorPollIntervalMs", qBound(40, ms, 1000));
}

// Manual driving
int Settings::speedLimit() const {
    return qBound(100, m_settings.value("control/speedLimit", 500).toInt(), 1000);
}
void Settings::setSpeedLimit(int stepsPerSecond) {
    m_settings.setValue("control/speedLimit", qBound(100, stepsPerSecond, 1000));
}

bool Settings::keyboardDriveEnabled() const {
    return m_settings.value("control/keyboardDrive", true).toBool();
}
void Settings::setKeyboardDriveEnabled(bool enabled) {
    m_settings.setValue("control/keyboardDrive", enabled);
}

int Settings::lastControlMode() const {
    return m_settings.value("general/lastControlMode", 0).toInt() == 1 ? 1 : 0;
}
void Settings::setLastControlMode(int mode) {
    m_settings.setValue("general/lastControlMode", mode == 1 ? 1 : 0);
}
