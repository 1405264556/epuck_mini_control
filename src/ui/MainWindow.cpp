#include "MainWindow.h"
#include "widgets/RobotListWidget.h"
#include "widgets/SensorDisplayWidget.h"
#include "widgets/RobotControlPanel.h"
#include "widgets/CoordinationCanvas.h"
#include "models/LogModel.h"
#include "dialogs/ConnectDialog.h"
#include "dialogs/FlashDialog.h"
#include "dialogs/FormationDialog.h"
#include "dialogs/SettingsDialog.h"
#include "dialogs/AboutDialog.h"

#include "app/Application.h"
#include "core/RobotManager.h"
#include "core/RobotInstance.h"
#include "core/Settings.h"
#include "comm/SerialManager.h"
#include "control/MultiRobotCoordinator.h"

#include <QMenuBar>
#include <QToolBar>
#include <QDockWidget>
#include <QStatusBar>
#include <QListView>
#include <QMessageBox>
#include <QCloseEvent>
#include <QLabel>
#include <QActionGroup>
#include <QStyle>
#include <QVBoxLayout>
#include <QTimer>
#include <QApplication>

MainWindow::MainWindow(RobotManager *robotMgr,
                       SerialManager *serialMgr,
                       MultiRobotCoordinator *coordinator, Settings *settings,
                       QWidget *parent)
    : QMainWindow(parent)
    , m_robotManager(robotMgr)
    , m_serialManager(serialMgr)
    , m_coordinator(coordinator)
    , m_settings(settings)
{
    setWindowTitle("e-puck Mini 控制中心 · 0.5.0");
    resize(1400, 900);
    setMinimumSize(1024, 700);

    setupMenuBar();
    setupToolBar();
    setupDockWidgets();
    setupStatusBar();
    setupConnections();
    restoreWindowState();

    statusBar()->showMessage("就绪", 3000);
}

MainWindow::~MainWindow() = default;

// ---- Menu Bar ----
void MainWindow::setupMenuBar() {
    auto *fileMenu = menuBar()->addMenu("文件(&F)");
    fileMenu->addAction("设置(&S)...", this, [this]() {
        auto dlg = new SettingsDialog(m_settings, this);
        dlg->exec();
        dlg->deleteLater();
    });
    fileMenu->addSeparator();
    fileMenu->addAction("退出(&X)", this, &QWidget::close, QKeySequence::Quit);

    auto *robotMenu = menuBar()->addMenu("机器人(&R)");
    m_scanAction = robotMenu->addAction("扫描机器人(&S)", this, [this]() {
        m_serialManager->scanPorts();
    }, QKeySequence("Ctrl+R"));
    m_connectAction = robotMenu->addAction("连接(&C)...", this, [this]() {
        showConnectDialog();
    }, QKeySequence("Ctrl+Shift+C"));
    m_disconnectAction = robotMenu->addAction("断开(&D)", this, [this]() {
        RobotId id = m_robotManager->selectedRobotId();
        if (!id.isEmpty()) {
            m_serialManager->disconnectRobot(id);
        }
    });
    robotMenu->addSeparator();
    m_flashAction = robotMenu->addAction("固件烧录(&F)...", this, [this]() {
        auto *dlg = new FlashDialog(m_robotManager,
                                    Application::self() ? Application::self()->dfuProgrammer() : nullptr,
                                    nullptr,
                                    this);
        dlg->exec();
        dlg->deleteLater();
    }, QKeySequence("Ctrl+F"));

    auto *controlMenu = menuBar()->addMenu("控制(&C)");
    m_formationAction = controlMenu->addAction("协调控制(&F)...", this, [this]() {
        setControlMode(1);
        m_controlPanel->showCoordinationPage();
    });
    controlMenu->addSeparator();
    m_emergencyStopAction = controlMenu->addAction("紧急停止(&E)", this, [this]() {
        m_controlPanel->stopManualControls();
        m_coordinator->emergencyStop();
        statusBar()->showMessage("紧急停止 - 所有电机已停止", 5000);
    }, QKeySequence("Escape"));

    auto *viewMenu = menuBar()->addMenu("视图(&V)");
    viewMenu->addAction("重置布局(&R)", this, [this]() {
        setControlMode(m_controlMode);
        m_logDock->hide();
        m_coordinationCanvas->fitAllRobots();
    });

    auto *helpMenu = menuBar()->addMenu("帮助(&H)");
    helpMenu->addAction("关于(&A)...", this, [this]() {
        auto dlg = new AboutDialog(this);
        dlg->exec();
        dlg->deleteLater();
    });
}

// ---- Tool Bar ----
void MainWindow::setupToolBar() {
    auto *toolbar = addToolBar("主工具栏");
    toolbar->setObjectName("main_toolbar");
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(20, 20));
    auto *brand = new QLabel("e-puck Mini");
    brand->setStyleSheet("font-size: 16px; font-weight: 600; padding: 0 12px 0 6px;");
    toolbar->addWidget(brand);
    auto *modeGroup = new QActionGroup(this);
    m_singleModeAction = toolbar->addAction("单机");
    m_multiModeAction = toolbar->addAction("多机");
    m_singleModeAction->setObjectName("workspaceSingle");
    m_multiModeAction->setObjectName("workspaceMulti");
    for (auto *action : {m_singleModeAction, m_multiModeAction}) {
        action->setCheckable(true);
        modeGroup->addAction(action);
    }
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(m_singleModeAction, &QAction::triggered, this, [this]() { setControlMode(0); });
    connect(m_multiModeAction, &QAction::triggered, this, [this]() { setControlMode(1); });
    toolbar->addSeparator();

    m_scanAction->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    toolbar->addAction(m_scanAction);

    m_connectAction->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    toolbar->addAction(m_connectAction);

    m_disconnectAction->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    toolbar->addAction(m_disconnectAction);

    toolbar->addSeparator();

    m_formationAction->setIcon(style()->standardIcon(QStyle::SP_FileDialogListView));
    toolbar->addAction(m_formationAction);

    toolbar->addSeparator();

    m_flashAction->setIcon(style()->standardIcon(QStyle::SP_DriveHDIcon));
    toolbar->addAction(m_flashAction);

    toolbar->addSeparator();

    // Emergency stop - prominent
    m_emergencyStopAction->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_emergencyStopAction->setToolTip("紧急停止 (Esc)");
    toolbar->addAction(m_emergencyStopAction);

    toolbar->addSeparator();

    m_settingsAction = toolbar->addAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView), "设置");
    connect(m_settingsAction, &QAction::triggered, this, [this]() {
        auto dlg = new SettingsDialog(m_settings, this);
        dlg->exec();
        dlg->deleteLater();
    });
}

// ---- Dock Widgets ----
void MainWindow::setupDockWidgets() {
    // --- Left Dock: Robot List ---
    m_robotListDock = new QDockWidget("机器人列表", this);
    m_robotListDock->setObjectName("dock_robot_list");
    m_robotListDock->setFeatures(QDockWidget::DockWidgetMovable
                                  | QDockWidget::DockWidgetFloatable);
    m_robotListWidget = new RobotListWidget(m_robotManager, m_serialManager, this);
    m_robotListDock->setWidget(m_robotListWidget);
    m_robotListDock->setMinimumWidth(205);
    addDockWidget(Qt::LeftDockWidgetArea, m_robotListDock);

    // --- Central: Coordination Canvas ---
    m_coordinationCanvas = new CoordinationCanvas(m_robotManager, this);
    m_coordinationCanvas->setObjectName("coordinationCanvas");
    auto *workspace = new QWidget;
    auto *mapLayout = new QVBoxLayout(workspace);
    mapLayout->setContentsMargins(0, 0, 0, 0);
    mapLayout->setSpacing(0);
    auto *mapToolbar = new QToolBar;
    mapToolbar->setIconSize(QSize(18, 18));
    mapToolbar->addWidget(new QLabel("  运动地图 · cm  "));
    auto *fit = mapToolbar->addAction(style()->standardIcon(QStyle::SP_TitleBarMaxButton), "适应地图");
    connect(fit, &QAction::triggered, m_coordinationCanvas, &CoordinationCanvas::fitAllRobots);
    m_goalAction = mapToolbar->addAction(style()->standardIcon(QStyle::SP_ArrowForward), "目标点");
    m_goalAction->setCheckable(true);
    m_obstacleAction = mapToolbar->addAction(style()->standardIcon(QStyle::SP_MessageBoxWarning), "虚拟障碍");
    m_obstacleAction->setCheckable(true);
    connect(m_goalAction, &QAction::toggled, this, [this](bool enabled) {
        if (enabled) m_obstacleAction->setChecked(false);
        m_coordinationCanvas->setGoalPickMode(enabled);
    });
    connect(m_obstacleAction, &QAction::toggled, this, [this](bool enabled) {
        if (enabled) m_goalAction->setChecked(false);
        m_coordinationCanvas->setObstacleEditMode(enabled);
    });
    auto *clear = mapToolbar->addAction(style()->standardIcon(QStyle::SP_TrashIcon), "清理画布");
    connect(clear, &QAction::triggered, m_coordinationCanvas, &CoordinationCanvas::clearCanvasOverlays);
    mapLayout->addWidget(mapToolbar);
    mapLayout->addWidget(m_coordinationCanvas, 1);
    setCentralWidget(workspace);

    // --- Right Dock: Sensor Display ---
    m_sensorDisplayDock = new QDockWidget("传感器显示", this);
    m_sensorDisplayDock->setObjectName("dock_sensor_display");
    m_sensorDisplayDock->setFeatures(QDockWidget::DockWidgetMovable
                                     | QDockWidget::DockWidgetFloatable);
    m_sensorDisplayWidget = new SensorDisplayWidget(m_robotManager, this);
    m_sensorDisplayDock->setWidget(m_sensorDisplayWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_sensorDisplayDock);

    // --- Right Dock (bottom): Robot Control ---
    m_controlPanelDock = new QDockWidget("机器人控制", this);
    m_controlPanelDock->setObjectName("dock_control_panel");
    m_controlPanelDock->setFeatures(QDockWidget::DockWidgetMovable
                                    | QDockWidget::DockWidgetFloatable);
    m_controlPanelDock->setMinimumWidth(330);
    m_controlPanel = new RobotControlPanel(m_robotManager, m_coordinator, this);
    m_controlPanelDock->setWidget(m_controlPanel);
    splitDockWidget(m_sensorDisplayDock, m_controlPanelDock, Qt::Vertical);

    // --- Bottom Dock: Log View ---
    m_logDock = new QDockWidget("日志", this);
    m_logDock->setObjectName("dock_log");
    m_logDock->setFeatures(QDockWidget::DockWidgetMovable
                           | QDockWidget::DockWidgetFloatable
                           | QDockWidget::DockWidgetClosable);
    m_logModel = new LogModel(this);
    m_logView = new QListView;
    m_logView->setModel(m_logModel);
    m_logView->setAlternatingRowColors(true);
    m_logView->setUniformItemSizes(true);
    m_logDock->setWidget(m_logView);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    m_logDock->hide();
    menuBar()->actions().at(3)->menu()->addAction(m_logDock->toggleViewAction());
    for (auto *dock : {m_robotListDock, m_sensorDisplayDock, m_controlPanelDock}) dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    setCorner(Qt::BottomLeftCorner, Qt::BottomDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);

    resizeDocks({m_robotListDock}, {260}, Qt::Horizontal);
    resizeDocks({m_sensorDisplayDock, m_controlPanelDock}, {300, 500}, Qt::Vertical);
    resizeDocks({m_logDock}, {150}, Qt::Vertical);
}

// ---- Status Bar ----
void MainWindow::setupStatusBar() {
    statusBar()->showMessage("就绪 - 请扫描 COM 端口连接机器人");
    m_onlineLabel = new QLabel;
    m_coordinationLabel = new QLabel;
    statusBar()->addPermanentWidget(m_onlineLabel);
    statusBar()->addPermanentWidget(m_coordinationLabel);
    auto *timer = new QTimer(this);
    timer->setInterval(250);
    connect(timer, &QTimer::timeout, this, &MainWindow::refreshStatus);
    timer->start();
    refreshStatus();
}

// ---- Connections ----
void MainWindow::setupConnections() {
    connect(m_robotListWidget, &RobotListWidget::connectRequested, this, [this](const RobotId &id) {
        requestRobotConnection(id);
    });
    connect(m_robotListWidget, &RobotListWidget::connectAllRequested, this,
            [this](const QList<RobotId> &ids) {
        int submitted = 0;
        for (const auto &id : ids) submitted += requestRobotConnection(id) ? 1 : 0;
        statusBar()->showMessage(QString("已提交 %1 台机器人的并行连接请求").arg(submitted), 5000);
    });
    connect(m_robotListWidget, &RobotListWidget::disconnectAllRequested, this,
            [this](const QList<RobotId> &ids) {
        for (const auto &id : ids) m_serialManager->disconnectRobot(id);
        statusBar()->showMessage(QString("正在断开 %1 台机器人").arg(ids.size()), 5000);
    });
    connect(m_serialManager, &SerialManager::robotConnected, this, [this](const RobotId &id) {
        statusBar()->showMessage(QString("%1 已连接，多机器人在线数：%2")
            .arg(id).arg(m_robotManager->connectedRobots().size()), 5000);
    });
    connect(m_serialManager, &SerialManager::errorOccurred, this,
            [this](const RobotId &id, const QString &message) {
        statusBar()->showMessage(QString("%1：%2").arg(id, message), 8000);
    });
    connect(m_serialManager, &SerialManager::telemetryStatus, this,
            [this](const RobotId &id, const QString &message) { statusBar()->showMessage(id + " · " + message, 5000); });
    connect(m_coordinator, &MultiRobotCoordinator::strategyError, this,
            [this](const QString &message) { statusBar()->showMessage(message, 8000); });
    connect(m_coordinationCanvas, &CoordinationCanvas::robotClicked, m_robotManager, &RobotManager::setSelectedRobot);

    connect(m_robotManager, &RobotManager::selectedRobotChanged, this, [this](const RobotId &id) {
        statusBar()->showMessage(QString("已选中: %1").arg(id), 2000);
    });

    connect(m_controlPanel, &RobotControlPanel::goalPickRequested, this, [this]() {
        m_goalAction->setChecked(true);
        statusBar()->showMessage("请在中央画布点击路径目标点", 5000);
    });
    connect(m_controlPanel, &RobotControlPanel::obstacleEditRequested, this, [this]() {
        m_obstacleAction->setChecked(true);
        statusBar()->showMessage("请在中央画布点击虚拟障碍位置", 5000);
    });
    connect(m_controlPanel, &RobotControlPanel::canvasClearRequested,
            m_coordinationCanvas, &CoordinationCanvas::clearCanvasOverlays);
    connect(m_coordinationCanvas, &CoordinationCanvas::goalPointSelected,
            m_controlPanel, &RobotControlPanel::setGoalFromCanvas);
    connect(m_coordinationCanvas, &CoordinationCanvas::virtualObstaclesChanged,
            m_controlPanel, &RobotControlPanel::setVirtualObstacles);
    connect(m_coordinationCanvas, &CoordinationCanvas::goalPointSelected, this, [this](const Vec2 &pos) {
        m_goalAction->setChecked(false);
        statusBar()->showMessage(QString("已选择目标点: X=%1, Y=%2")
            .arg(pos.x, 0, 'f', 1)
            .arg(pos.y, 0, 'f', 1), 3000);
    });
    connect(m_controlPanel, &RobotControlPanel::pathPlanned,
            m_coordinationCanvas, &CoordinationCanvas::showPath);

    connect(Logger::instance(), &Logger::newLogEntry, m_logModel, &LogModel::appendEntry);
}

// ---- Window State Persistence ----
void MainWindow::saveWindowState() {
    if (m_settings) {
        m_settings->setMainWindowGeometry(saveGeometry());
        m_settings->setMainWindowState(saveState(5));
    }
}

void MainWindow::restoreWindowState() {
    if (m_settings) {
        QByteArray geo = m_settings->mainWindowGeometry();
        if (!geo.isEmpty()) restoreGeometry(geo);
        QByteArray state = m_settings->mainWindowState();
        if (!state.isEmpty()) restoreState(state, 5);
    }
    setControlMode(0);
}

void MainWindow::showConnectDialog() {
    auto *dlg = new ConnectDialog(m_serialManager, this);
    if (dlg->exec() == QDialog::Accepted) {
        BLEDeviceInfo info = dlg->selectedDevice();
        RobotId id = info.deriveId();
        QString port = info.portName;

        if (port.isEmpty()) {
            statusBar()->showMessage("错误: 无效的端口名", 5000);
            dlg->deleteLater();
            return;
        }

        if (m_serialManager->connectToPort(port, id, info.baudRate)) {
            statusBar()->showMessage(QString("正在后台连接 %1...").arg(port), 5000);
        } else {
            statusBar()->showMessage(QString("COM 端口 %1 连接失败").arg(port), 5000);
        }
    }
    dlg->deleteLater();
}

bool MainWindow::requestRobotConnection(const QString &id) {
    auto *robot = m_robotManager->robot(id);
    if (!robot) return false;

    const auto &info = robot->deviceInfo();
    if (info.portName.isEmpty()) {
        statusBar()->showMessage("错误：无效的端口名", 5000);
        return false;
    }

    statusBar()->showMessage(QString("正在后台连接 %1...").arg(info.portName));
    if (!m_serialManager->connectToPort(info.portName, id, info.baudRate)) {
        statusBar()->showMessage(QString("COM 端口 %1 连接请求失败").arg(info.portName), 5000);
        return false;
    }
    return true;
}

void MainWindow::closeEvent(QCloseEvent *event) {
    m_controlPanel->stopManualControls();
    m_coordinator->emergencyStop();
    saveWindowState();
    event->accept();
}

void MainWindow::setControlMode(int mode) {
    m_controlMode = mode == 1 ? 1 : 0;
    m_controlPanel->setControlMode(m_controlMode);
    m_singleModeAction->setChecked(m_controlMode == 0);
    m_multiModeAction->setChecked(m_controlMode == 1);
    m_sensorDisplayWidget->setMultiRobotMode(m_controlMode == 1);
    removeDockWidget(m_controlPanelDock);
    m_controlPanelDock->setMinimumWidth(m_controlMode == 1 ? 0 : 330);
    m_controlPanelDock->setWindowTitle(m_controlMode == 1 ? "多机器人 · 同步与分控" : "单机器人 · 驱动与任务");
    if (m_controlMode == 1) {
        addDockWidget(Qt::BottomDockWidgetArea, m_controlPanelDock);
        resizeDocks({m_controlPanelDock}, {340}, Qt::Vertical);
    } else {
        addDockWidget(Qt::RightDockWidgetArea, m_controlPanelDock);
        splitDockWidget(m_sensorDisplayDock, m_controlPanelDock, Qt::Vertical);
        resizeDocks({m_sensorDisplayDock, m_controlPanelDock}, {250, 400}, Qt::Vertical);
    }
    m_controlPanelDock->show();
    resizeDocks({m_robotListDock}, {210}, Qt::Horizontal);
    resizeDocks({m_sensorDisplayDock}, {350}, Qt::Horizontal);
    QTimer::singleShot(0, m_coordinationCanvas, &CoordinationCanvas::fitAllRobots);
}

void MainWindow::refreshStatus() {
    m_onlineLabel->setText(QString("在线 %1 / %2  ").arg(m_robotManager->connectedRobots().size()).arg(m_robotManager->robotCount()));
    m_coordinationLabel->setText(m_coordinator->isPaused() ? "协调已暂停  " : m_coordinator->isActive() ? "协调运行中  " : "手动控制  ");
}

void MainWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::WindowDeactivate && m_controlPanel) m_controlPanel->stopManualControls();
    QMainWindow::changeEvent(event);
}
