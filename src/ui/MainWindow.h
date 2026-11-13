#pragma once

#include <QMainWindow>
#include <QSet>
#include "core/Types.h"

class QDockWidget;
class QListView;
class QAction;
class QLabel;
class QToolButton;

class RobotManager;
class SerialManager;
class Settings;
class MultiRobotCoordinator;

class RobotListWidget;
class SensorDisplayWidget;
class RobotControlPanel;
class CoordinationCanvas;
class LogModel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(RobotManager *robotMgr,
                        SerialManager *serialMgr,
                        MultiRobotCoordinator *coordinator, Settings *settings,
                        QWidget *parent = nullptr);
    ~MainWindow() override;
    void setControlMode(int mode);
    int controlMode() const { return m_controlMode; }
    void setKeyboardDriveEnabled(bool enabled);
    bool keyboardDriveEnabled() const { return m_keyboardDrive; }
    void emergencyStop();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void showConnectDialog();
    void showSettingsDialog();
    bool requestRobotConnection(const QString &id);
    void setupMenuBar();
    void setupToolBar();
    void setupDockWidgets();
    void setupStatusBar();
    void setupConnections();
    void saveWindowState();
    void restoreWindowState();
    void applySettings();
    void loadKnownDevices();
    void saveKnownDevices();
    void refreshStatus();
    QString robotLabel(const RobotId &id) const;
    bool handleDriveKey(int key, bool pressed, bool autoRepeat);
    void releaseDriveKeys();
    void updateKeyboardDrive();

    // Dependencies
    RobotManager *m_robotManager;
    SerialManager *m_serialManager;
    MultiRobotCoordinator *m_coordinator;
    Settings *m_settings;

    // Dock widgets
    QDockWidget *m_robotListDock = nullptr;
    QDockWidget *m_sensorDisplayDock = nullptr;
    QDockWidget *m_controlPanelDock = nullptr;
    QDockWidget *m_logDock = nullptr;

    // Contained widgets
    RobotListWidget *m_robotListWidget = nullptr;
    SensorDisplayWidget *m_sensorDisplayWidget = nullptr;
    RobotControlPanel *m_controlPanel = nullptr;
    CoordinationCanvas *m_coordinationCanvas = nullptr;
    QListView *m_logView = nullptr;
    LogModel *m_logModel = nullptr;

    // Actions
    QAction *m_scanAction = nullptr;
    QAction *m_connectAction = nullptr;
    QAction *m_disconnectAction = nullptr;
    QAction *m_flashAction = nullptr;
    QAction *m_formationAction = nullptr;
    QAction *m_emergencyStopAction = nullptr;
    QAction *m_settingsAction = nullptr;
    QAction *m_keyboardAction = nullptr;
    QAction *m_singleModeAction = nullptr;
    QAction *m_multiModeAction = nullptr;
    QAction *m_goalAction = nullptr;
    QAction *m_obstacleAction = nullptr;
    QToolButton *m_emergencyStopButton = nullptr;
    QLabel *m_onlineLabel = nullptr;
    QLabel *m_coordinationLabel = nullptr;
    QLabel *m_keyboardLabel = nullptr;
    QLabel *m_cursorLabel = nullptr;
    int m_controlMode = 0;
    bool m_keyboardDrive = true;
    bool m_logAtBottom = true;
    QSet<int> m_driveKeys;
    QString m_singleSummary;
    void refreshControlDockTitle();
};
