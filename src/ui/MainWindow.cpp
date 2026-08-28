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
    setWindowTitle("e-puck Mini 控制中心");
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
        showConnectDialog();
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
    m_formationAction = controlMenu->addAction("编队控制(&F)...", this, []() {
        // Will be wired in Phase 5
    });
    controlMenu->addSeparator();
    m_emergencyStopAction = controlMenu->addAction("紧急停止(&E)", this, [this]() {
        m_coordinator->emergencyStop();
        statusBar()->showMessage("紧急停止 - 所有电机已停止", 5000);
    }, QKeySequence("Escape"));

    auto *viewMenu = menuBar()->addMenu("视图(&V)");
    viewMenu->addAction("重置布局(&R)", this, [this]() {
        // Restore default dock arrangement
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

    m_scanAction->setIcon(QIcon(":/icons/connect.svg"));
    toolbar->addAction(m_scanAction);

    m_connectAction->setIcon(QIcon(":/icons/connect.svg"));
    toolbar->addAction(m_connectAction);

    m_disconnectAction->setIcon(QIcon(":/icons/disconnect.svg"));
    toolbar->addAction(m_disconnectAction);

    toolbar->addSeparator();

    m_formationAction->setIcon(QIcon(":/icons/formation.svg"));
    toolbar->addAction(m_formationAction);

    toolbar->addSeparator();

    m_flashAction->setIcon(QIcon(":/icons/flash.svg"));
    toolbar->addAction(m_flashAction);

    toolbar->addSeparator();

    // Emergency stop - prominent
    m_emergencyStopAction->setIcon(QIcon(":/icons/stop.svg"));
    m_emergencyStopAction->setToolTip("紧急停止 (Esc)");
    toolbar->addAction(m_emergencyStopAction);

    toolbar->addSeparator();

    m_settingsAction = toolbar->addAction(QIcon(":/icons/settings.svg"), "设置");
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
    addDockWidget(Qt::LeftDockWidgetArea, m_robotListDock);

    // --- Central: Coordination Canvas ---
    m_coordinationCanvas = new CoordinationCanvas(m_robotManager, this);
    setCentralWidget(m_coordinationCanvas);

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
    m_controlPanelDock->setMinimumWidth(430);
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

    resizeDocks({m_robotListDock}, {260}, Qt::Horizontal);
    resizeDocks({m_sensorDisplayDock, m_controlPanelDock}, {300, 500}, Qt::Vertical);
    resizeDocks({m_logDock}, {150}, Qt::Vertical);
}

// ---- Status Bar ----
void MainWindow::setupStatusBar() {
    statusBar()->showMessage("就绪 - 请扫描 COM 端口连接机器人");
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

    connect(m_robotManager, &RobotManager::selectedRobotChanged, this, [this](const RobotId &id) {
        statusBar()->showMessage(QString("已选中: %1").arg(id), 2000);
    });

    connect(m_controlPanel, &RobotControlPanel::goalPickRequested, this, [this]() {
        m_coordinationCanvas->setGoalPickMode(true);
        statusBar()->showMessage("请在中央画布点击路径目标点", 5000);
    });
    connect(m_controlPanel, &RobotControlPanel::obstacleEditRequested, this, [this]() {
        m_coordinationCanvas->setObstacleEditMode(true);
        statusBar()->showMessage("请在中央画布点击虚拟障碍位置", 5000);
    });
    connect(m_controlPanel, &RobotControlPanel::canvasClearRequested,
            m_coordinationCanvas, &CoordinationCanvas::clearCanvasOverlays);
    connect(m_coordinationCanvas, &CoordinationCanvas::goalPointSelected,
            m_controlPanel, &RobotControlPanel::setGoalFromCanvas);
    connect(m_coordinationCanvas, &CoordinationCanvas::virtualObstaclesChanged,
            m_controlPanel, &RobotControlPanel::setVirtualObstacles);
    connect(m_coordinationCanvas, &CoordinationCanvas::goalPointSelected, this, [this](const Vec2 &pos) {
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
        m_settings->setMainWindowState(saveState());
    }
}

void MainWindow::restoreWindowState() {
    if (m_settings) {
        QByteArray geo = m_settings->mainWindowGeometry();
        if (!geo.isEmpty()) restoreGeometry(geo);
        QByteArray state = m_settings->mainWindowState();
        if (!state.isEmpty()) restoreState(state);
    }
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
    saveWindowState();
    event->accept();
}
