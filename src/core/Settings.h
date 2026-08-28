#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QByteArray>

class Settings : public QObject {
    Q_OBJECT
public:
    explicit Settings(QObject *parent = nullptr);

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

    // General
    QString lastFirmwareDirectory() const;
    void setLastFirmwareDirectory(const QString &dir);

private:
    QSettings m_settings;
};
