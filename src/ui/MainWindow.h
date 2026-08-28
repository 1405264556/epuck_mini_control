#pragma once

#include <QMainWindow>

class QDockWidget;
class QListView;
class QAction;
class QLabel;

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

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void showConnectDialog();
    bool requestRobotConnection(const QString &id);
    void setupMenuBar();
    void setupToolBar();
    void setupDockWidgets();
    void setupStatusBar();
    void setupConnections();
    void saveWindowState();
    void restoreWindowState();

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
};
