#include "Settings.h"

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
