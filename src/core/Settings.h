#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QByteArray>
#include <QList>

class Settings : public QObject {
    Q_OBJECT
public:
    explicit Settings(QObject *parent = nullptr);
    // Stores everything in the given INI file instead of the per-user location.
    explicit Settings(const QString &iniFilePath, QObject *parent = nullptr);

    // BLE
    int bleScanTimeoutMs() const;
    void setBleScanTimeoutMs(int ms);
    int bleReconnectAttempts() const;
    void setBleReconnectAttempts(int n);
    int bleRssiThreshold() const;
    void setBleRssiThreshold(int dbm);

    // Display
    int displayRefreshRateHz() const;
    void setDisplayRefreshRateHz(int hz);
    int chartHistorySeconds() const;
    void setChartHistorySeconds(int s);

    // Control
    double defaultMotorSpeed() const;
    void setDefaultMotorSpeed(double speed);
    double joystickSensitivity() const;
    void setJoystickSensitivity(double s);
    int controlLoopRateHz() const;
    void setControlLoopRateHz(int hz);

    // Logging
    int logLevel() const;
    void setLogLevel(int level);

    // Window state
    QByteArray mainWindowGeometry() const;
    void setMainWindowGeometry(const QByteArray &geo);
    QByteArray mainWindowState() const;
    void setMainWindowState(const QByteArray &state);

    // Connection
    struct KnownDevice {
        QString portName;
        QString name;
        int baudRate = 115200;
        bool wasConnected = false;   // linked when last saved; used for auto-connect
    };
    QList<KnownDevice> knownDevices() const;
    void setKnownDevices(const QList<KnownDevice> &devices);
    bool autoReconnect() const;
    void setAutoReconnect(bool enabled);
    int reconnectAttempts() const;
    void setReconnectAttempts(int attempts);
    bool autoConnectOnStartup() const;
    void setAutoConnectOnStartup(bool enabled);
    int sensorPollIntervalMs() const;
    void setSensorPollIntervalMs(int ms);

    // Manual driving
    int speedLimit() const;            // steps/s, 100..1000
    void setSpeedLimit(int stepsPerSecond);
    bool keyboardDriveEnabled() const;
    void setKeyboardDriveEnabled(bool enabled);
    int lastControlMode() const;
    void setLastControlMode(int mode);

    // General
    QString lastFirmwareDirectory() const;
    void setLastFirmwareDirectory(const QString &dir);

private:
    QSettings m_settings;
};
