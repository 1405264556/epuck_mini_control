#include "MainWindow.h"
#include "Icons.h"
#include "widgets/RobotListWidget.h"
#include "widgets/SensorDisplayWidget.h"
#include "widgets/RobotControlPanel.h"
#include "widgets/CoordinationCanvas.h"
#include "models/LogModel.h"
#include "dialogs/ConnectDialog.h"
#include "dialogs/FlashDialog.h"
#include "dialogs/SettingsDialog.h"
#include "dialogs/AboutDialog.h"

#include "app/Application.h"
#include "core/RobotManager.h"
#include "core/RobotInstance.h"
#include "core/Settings.h"
#include "comm/SerialManager.h"
#include "control/MultiRobotCoordinator.h"
#include "util/Logger.h"

#include <QAbstractSpinBox>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QStatusBar>
#include <QStyle>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#ifndef EPUCK_VERSION
#define EPUCK_VERSION "dev"
#endif

namespace {
// Bump when the dock arrangement changes so an old saved layout is not applied.
constexpr int kLayoutVersion = 6;

QLabel *makeChip(const QString &toolTip) {
    auto *label = new QLabel;
    label->setProperty("chip", true);
    label->setToolTip(toolTip);
    return label;
}

void setChip(QLabel *label, const QString &text, const char *tone) {
    if (label->text() != text) label->setText(text);
    if (label->property("tone").toString() == QLatin1String(tone)) return;
    label->setProperty("tone", tone);
    label->style()->unpolish(label);
    label->style()->polish(label);
}

// Widgets where letters and space are text input, not driving.
bool isTextEntry(const QWidget *widget) {
    return qobject_cast<const QLineEdit *>(widget)
        || qobject_cast<const QAbstractSpinBox *>(widget)
        || qobject_cast<const QComboBox *>(widget)
        || qobject_cast<const QTextEdit *>(widget)
        || qobject_cast<const QPlainTextEdit *>(widget);
}
}

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
    setWindowTitle(QString("e-puck Mini 控制中心 · %1").arg(EPUCK_VERSION));
    resize(1400, 900);
    setMinimumSize(1024, 700);

    setupMenuBar();
    setupToolBar();
    setupDockWidgets();
    setupStatusBar();
    setupConnections();
    loadKnownDevices();
    restoreWindowState();
    applySettings();

    // Driving keys are read application-wide so they work wherever the focus is,
    // except in text inputs (see eventFilter).
    qApp->installEventFilter(this);
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if (!m_driveKeys.isEmpty() && now && isTextEntry(now)) releaseDriveKeys();
    });

    statusBar()->showMessage(m_robotManager->robotCount() > 0
        ? "就绪 - 双击左侧设备即可连接"
        : "就绪 - 点击「扫描设备」查找机器人");

    if (m_settings && m_settings->autoConnectOnStartup()) {
        // After the window is up, so a slow Bluetooth open never delays startup.
        QTimer::singleShot(400, this, [this]() {
            int submitted = 0;
            for (const auto &device : m_settings->knownDevices()) {
                if (!device.wasConnected) continue;
                BLEDeviceInfo info;
                info.portName = device.portName;
                submitted += requestRobotConnection(info.deriveId()) ? 1 : 0;
            }
            if (submitted) {
                statusBar()->showMessage(QString("正在自动连接上次使用的 %1 台设备…").arg(submitted), 5000);
            }
        });
    }
}

MainWindow::~MainWindow() {
    qApp->removeEventFilter(this);
}

// ---- Menu Bar ----
void MainWindow::setupMenuBar() {
    auto *fileMenu = menuBar()->addMenu("文件(&F)");
    m_settingsAction = fileMenu->addAction(icons::make(icons::Glyph::Settings), "设置(&S)…",
                                           this, &MainWindow::showSettingsDialog);
    fileMenu->addSeparator();
    fileMenu->addAction("退出(&X)", QKeySequence::Quit, this, &QWidget::close);

    auto *robotMenu = menuBar()->addMenu("机器人(&R)");
    m_scanAction = robotMenu->addAction(icons::make(icons::Glyph::Scan), "扫描设备(&S)",
                                        QKeySequence("Ctrl+R"), this, [this]() {
        if (m_serialManager->isScanning()) m_serialManager->stopScan();
        else m_serialManager->scanPorts();
    });
    m_connectAction = robotMenu->addAction(icons::make(icons::Glyph::Link), "手动连接(&C)…",
                                           QKeySequence("Ctrl+Shift+C"), this,
                                           &MainWindow::showConnectDialog);
    m_disconnectAction = robotMenu->addAction(icons::make(icons::Glyph::Unlink), "断开当前(&D)",
                                              this, [this]() {
        const RobotId id = m_robotManager->selectedRobotId();
        if (!id.isEmpty()) m_serialManager->disconnectRobot(id);
    });
    robotMenu->addSeparator();
    m_flashAction = robotMenu->addAction(icons::make(icons::Glyph::Flash), "固件烧录(&F)…",
                                         QKeySequence("Ctrl+F"), this, [this]() {
        auto *dlg = new FlashDialog(m_robotManager,
                                    Application::self() ? Application::self()->dfuProgrammer() : nullptr,
                                    nullptr,
                                    this);
        dlg->exec();
        dlg->deleteLater();
    });

    auto *controlMenu = menuBar()->addMenu("控制(&C)");
    m_formationAction = controlMenu->addAction(icons::make(icons::Glyph::Fleet), "协调控制(&F)…",
                                               this, [this]() {
        setControlMode(1);
        m_controlPanel->showCoordinationPage();
    });
    m_keyboardAction = controlMenu->addAction(icons::make(icons::Glyph::Keyboard), "键盘驾驶 W A S D(&K)");
    m_keyboardAction->setCheckable(true);
    m_keyboardAction->setChecked(m_keyboardDrive);
    m_keyboardAction->setShortcut(QKeySequence("Ctrl+K"));
    connect(m_keyboardAction, &QAction::toggled, this, &MainWindow::setKeyboardDriveEnabled);
    controlMenu->addSeparator();
    m_emergencyStopAction = controlMenu->addAction(
        icons::make(icons::Glyph::Stop, QColor(icons::kDanger)), "紧急停止(&E)",
        QKeySequence(Qt::Key_Escape), this, &MainWindow::emergencyStop);

    auto *viewMenu = menuBar()->addMenu("视图(&V)");
    viewMenu->setObjectName("viewMenu");
    viewMenu->addAction("重置布局(&R)", this, [this]() {
        setControlMode(m_controlMode);
        m_logDock->hide();
        m_coordinationCanvas->fitAllRobots();
    });

    auto *helpMenu = menuBar()->addMenu("帮助(&H)");
    helpMenu->addAction("关于(&A)…", this, [this]() {
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
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->setContextMenuPolicy(Qt::PreventContextMenu);

    auto *brand = new QLabel("e-puck Mini");
    brand->setObjectName("brandLabel");
    toolbar->addWidget(brand);

    auto *modeGroup = new QActionGroup(this);
    m_singleModeAction = toolbar->addAction(icons::make(icons::Glyph::Single), "单机");
    m_multiModeAction = toolbar->addAction(icons::make(icons::Glyph::Fleet), "多机");
    m_singleModeAction->setObjectName("workspaceSingle");
    m_multiModeAction->setObjectName("workspaceMulti");
    m_singleModeAction->setToolTip("单机模式：驾驶、LED、路径规划");
    m_multiModeAction->setToolTip("多机模式：同步组与逐台分控、协调算法");
    for (auto *action : {m_singleModeAction, m_multiModeAction}) {
        action->setCheckable(true);
        modeGroup->addAction(action);
    }
    connect(m_singleModeAction, &QAction::triggered, this, [this]() { setControlMode(0); });
    connect(m_multiModeAction, &QAction::triggered, this, [this]() { setControlMode(1); });
    toolbar->addSeparator();

    // Toolbar buttons show a short label; the menu keeps the full text with mnemonics.
    auto addWithLabel = [toolbar](QAction *action, const QString &label, const QString &tip) {
        action->setIconText(label);
        action->setToolTip(tip);
        toolbar->addAction(action);
    };
    addWithLabel(m_scanAction, "扫描", "扫描已配对的蓝牙 / USB 串口 (Ctrl+R)");
    addWithLabel(m_connectAction, "手动连接", "输入端口名直接连接 (Ctrl+Shift+C)");
    toolbar->addSeparator();
    addWithLabel(m_formationAction, "协调", "切换到多机模式的协调算法页");
    addWithLabel(m_flashAction, "烧录", "固件烧录 (Ctrl+F)");
    addWithLabel(m_keyboardAction, "键盘", "键盘驾驶：W A S D 移动，空格停止 (Ctrl+K 开关)");

    auto *spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);

    addWithLabel(m_settingsAction, "设置", "连接与驾驶设置");

    // The one control that must be findable instantly: always visible, far right, red.
    m_emergencyStopButton = new QToolButton;
    m_emergencyStopButton->setObjectName("emergencyStop");
    m_emergencyStopButton->setText("急停  Esc");
    m_emergencyStopButton->setIcon(icons::make(icons::Glyph::Stop, Qt::white));
    m_emergencyStopButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_emergencyStopButton->setToolTip("立即停止所有机器人并结束协调算法 (Esc)");
    m_emergencyStopButton->setFocusPolicy(Qt::NoFocus);
    connect(m_emergencyStopButton, &QToolButton::clicked, this, &MainWindow::emergencyStop);
    toolbar->addWidget(m_emergencyStopButton);
}

// ---- Dock Widgets ----
void MainWindow::setupDockWidgets() {
    // --- Left Dock: Robot List ---
    m_robotListDock = new QDockWidget("设备", this);
    m_robotListDock->setObjectName("dock_robot_list");
    m_robotListWidget = new RobotListWidget(m_robotManager, m_serialManager, this);
    m_robotListDock->setWidget(m_robotListWidget);
    m_robotListDock->setMinimumWidth(215);
    addDockWidget(Qt::LeftDockWidgetArea, m_robotListDock);

    // --- Central: Coordination Canvas ---
    m_coordinationCanvas = new CoordinationCanvas(m_robotManager, this);
    m_coordinationCanvas->setObjectName("coordinationCanvas");
    auto *workspace = new QWidget;
    auto *mapLayout = new QVBoxLayout(workspace);
    mapLayout->setContentsMargins(0, 0, 0, 0);
    mapLayout->setSpacing(0);
    auto *mapToolbar = new QToolBar;
    mapToolbar->setObjectName("mapToolbar");
    mapToolbar->setIconSize(QSize(18, 18));
    mapToolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    auto *mapTitle = new QLabel("运动地图");
    mapTitle->setObjectName("sectionLabel");
    mapToolbar->addWidget(mapTitle);
    auto *fit = mapToolbar->addAction(icons::make(icons::Glyph::Fit), "适应");
    fit->setToolTip("缩放到全部机器人和路径（双击地图空白处）");
    connect(fit, &QAction::triggered, m_coordinationCanvas, &CoordinationCanvas::fitAllRobots);
    m_goalAction = mapToolbar->addAction(icons::make(icons::Glyph::Target), "目标点");
    m_goalAction->setToolTip("在地图上点选路径目标点");
    m_goalAction->setCheckable(true);
    m_obstacleAction = mapToolbar->addAction(icons::make(icons::Glyph::Obstacle), "障碍");
    m_obstacleAction->setToolTip("在地图上连续放置虚拟障碍，再次点击结束");
    m_obstacleAction->setCheckable(true);
    connect(m_goalAction, &QAction::toggled, this, [this](bool enabled) {
        if (enabled) m_obstacleAction->setChecked(false);
        m_coordinationCanvas->setGoalPickMode(enabled);
    });
    connect(m_obstacleAction, &QAction::toggled, this, [this](bool enabled) {
        if (enabled) m_goalAction->setChecked(false);
        m_coordinationCanvas->setObstacleEditMode(enabled);
    });
    auto *clear = mapToolbar->addAction(icons::make(icons::Glyph::Trash), "清理");
    clear->setToolTip("清除路径、轨迹和虚拟障碍");
    connect(clear, &QAction::triggered, m_coordinationCanvas, &CoordinationCanvas::clearCanvasOverlays);
    auto *mapSpacer = new QWidget;
    mapSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    mapToolbar->addWidget(mapSpacer);
    m_cursorLabel = new QLabel("单位 cm");
    m_cursorLabel->setObjectName("valueReadout");
    m_cursorLabel->setMinimumWidth(170);
    m_cursorLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    mapToolbar->addWidget(m_cursorLabel);
    mapLayout->addWidget(mapToolbar);
    mapLayout->addWidget(m_coordinationCanvas, 1);
    setCentralWidget(workspace);

    // --- Right Dock: Sensor Display ---
    m_sensorDisplayDock = new QDockWidget("传感器", this);
    m_sensorDisplayDock->setObjectName("dock_sensor_display");
    m_sensorDisplayWidget = new SensorDisplayWidget(m_robotManager, this);
    m_sensorDisplayDock->setWidget(m_sensorDisplayWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_sensorDisplayDock);

    // --- Right Dock (bottom): Robot Control ---
    m_controlPanelDock = new QDockWidget("机器人控制", this);
    m_controlPanelDock->setObjectName("dock_control_panel");
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
    m_logView->setObjectName("logView");
    m_logView->setModel(m_logModel);
    m_logView->setUniformItemSizes(true);
    m_logView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_logView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_logDock->setWidget(m_logView);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    m_logDock->hide();
    // Follow new entries only while the view is already at the end, so scrolling
    // back to read something is not undone by the next log line.
    connect(m_logModel, &QAbstractItemModel::rowsAboutToBeInserted, this, [this]() {
        auto *bar = m_logView->verticalScrollBar();
        m_logAtBottom = bar->value() >= bar->maximum() - 2;
    });
    connect(m_logModel, &QAbstractItemModel::rowsInserted, this, [this]() {
        if (m_logAtBottom && m_logDock->isVisible()) m_logView->scrollToBottom();
    });
    connect(m_logDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (visible) m_logView->scrollToBottom();
    });
    auto *logAction = m_logDock->toggleViewAction();
    logAction->setText("日志面板(&L)");
    logAction->setIcon(icons::make(icons::Glyph::Log));
    logAction->setShortcut(QKeySequence("Ctrl+L"));
    if (auto *viewMenu = menuBar()->findChild<QMenu *>("viewMenu")) viewMenu->addAction(logAction);

    for (auto *dock : {m_robotListDock, m_sensorDisplayDock, m_controlPanelDock}) {
        dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    }
    setCorner(Qt::BottomLeftCorner, Qt::BottomDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);

    resizeDocks({m_robotListDock}, {240}, Qt::Horizontal);
    resizeDocks({m_sensorDisplayDock, m_controlPanelDock}, {300, 500}, Qt::Vertical);
    resizeDocks({m_logDock}, {150}, Qt::Vertical);
}

// ---- Status Bar ----
void MainWindow::setupStatusBar() {
    m_keyboardLabel = makeChip("键盘驾驶：W A S D 移动，空格停止，Ctrl+K 开关");
    m_onlineLabel = makeChip("已连接 / 列表中的机器人数量");
    m_coordinationLabel = makeChip("当前控制方式");
    statusBar()->addPermanentWidget(m_keyboardLabel);
    statusBar()->addPermanentWidget(m_coordinationLabel);
    statusBar()->addPermanentWidget(m_onlineLabel);
    statusBar()->setSizeGripEnabled(false);
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
        statusBar()->showMessage(QString("正在并行连接 %1 台机器人…").arg(submitted), 5000);
    });
    connect(m_robotListWidget, &RobotListWidget::disconnectAllRequested, this,
            [this](const QList<RobotId> &ids) {
        for (const auto &id : ids) m_serialManager->disconnectRobot(id);
        statusBar()->showMessage(QString("正在断开 %1 台机器人").arg(ids.size()), 5000);
    });
    connect(m_robotListWidget, &RobotListWidget::manualConnectRequested,
            this, &MainWindow::showConnectDialog);
    connect(m_robotListWidget, &RobotListWidget::removeRequested, this, [this](const RobotId &id) {
        auto *robot = m_robotManager->robot(id);
        if (!robot || m_serialManager->isRobotConnected(id)
            || robot->state() == RobotConnectionState::Connecting
            || robot->state() == RobotConnectionState::Disconnecting) return;
        const QString label = robotLabel(id);
        m_robotManager->removeRobot(id);
        saveKnownDevices();
        statusBar()->showMessage(QString("已从列表移除 %1").arg(label), 4000);
    });

    connect(m_serialManager, &SerialManager::scanStarted, this, [this]() {
        m_scanAction->setIconText("停止扫描");
        statusBar()->showMessage("正在后台扫描端口，已连接的设备不受影响…");
    });
    connect(m_serialManager, &SerialManager::scanFinished, this, [this]() {
        m_scanAction->setIconText("扫描");
        int found = 0;
        for (const auto &device : m_serialManager->discoveredDevices()) found += device.isEpuckMini ? 1 : 0;
        statusBar()->showMessage(found > 0
            ? QString("扫描完成：识别到 %1 台机器人，双击设备或点「连接全部」").arg(found)
            : QString("扫描完成：没有识别到机器人，请检查配对和端口占用"), 8000);
    });
    connect(m_serialManager, &SerialManager::robotConnected, this, [this](const RobotId &id) {
        statusBar()->showMessage(QString("%1 已连接 · 在线 %2 台")
            .arg(robotLabel(id)).arg(m_robotManager->connectedRobots().size()), 5000);
        saveKnownDevices();
    });
    connect(m_serialManager, &SerialManager::errorOccurred, this,
            [this](const RobotId &id, const QString &message) {
        statusBar()->showMessage(QString("%1：%2").arg(robotLabel(id), message), 10000);
    });
    connect(m_serialManager, &SerialManager::telemetryStatus, this,
            [this](const RobotId &id, const QString &message) {
        statusBar()->showMessage(robotLabel(id) + " · " + message, 5000);
    });
    connect(m_coordinator, &MultiRobotCoordinator::strategyError, this,
            [this](const QString &message) { statusBar()->showMessage(message, 8000); });
    // An algorithm owns the map pose while it runs; dragging a robot then would
    // feed it a false position.
    connect(m_coordinator, &MultiRobotCoordinator::coordinationStarted, this,
            [this]() { releaseDriveKeys(); m_coordinationCanvas->setRobotDragEnabled(false); });
    connect(m_coordinator, &MultiRobotCoordinator::coordinationStopped, this,
            [this]() { m_coordinationCanvas->setRobotDragEnabled(true); });
    connect(m_coordinationCanvas, &CoordinationCanvas::robotClicked, m_robotManager, &RobotManager::setSelectedRobot);
    connect(m_coordinationCanvas, &CoordinationCanvas::cursorMoved, this, [this](const Vec2 &pos) {
        m_cursorLabel->setText(QString("X %1  Y %2 cm").arg(pos.x, 7, 'f', 1).arg(pos.y, 7, 'f', 1));
    });
    connect(m_coordinationCanvas, &CoordinationCanvas::cursorLeft, this,
            [this]() { m_cursorLabel->setText("单位 cm"); });

    connect(m_controlPanel, &RobotControlPanel::singleRobotSummaryChanged, this,
            [this](const QString &summary) { m_singleSummary = summary; refreshControlDockTitle(); });
    connect(m_controlPanel, &RobotControlPanel::goalPickRequested, this, [this]() {
        m_goalAction->setChecked(true);
        statusBar()->showMessage("请在地图上点击路径目标点", 5000);
    });
    connect(m_controlPanel, &RobotControlPanel::obstacleEditRequested, this, [this]() {
        m_obstacleAction->setChecked(true);
        statusBar()->showMessage("请在地图上点击放置虚拟障碍，可连续添加", 5000);
    });
    connect(m_controlPanel, &RobotControlPanel::canvasClearRequested,
            m_coordinationCanvas, &CoordinationCanvas::clearCanvasOverlays);
    connect(m_controlPanel, &RobotControlPanel::speedLimitChanged, this, [this](int value) {
        if (m_settings) m_settings->setSpeedLimit(value);
    });
    connect(m_coordinationCanvas, &CoordinationCanvas::goalPointSelected,
            m_controlPanel, &RobotControlPanel::setGoalFromCanvas);
    connect(m_coordinationCanvas, &CoordinationCanvas::virtualObstaclesChanged,
            m_controlPanel, &RobotControlPanel::setVirtualObstacles);
    connect(m_coordinationCanvas, &CoordinationCanvas::goalPointSelected, this, [this](const Vec2 &pos) {
        m_goalAction->setChecked(false);
        statusBar()->showMessage(QString("已选择目标点 X=%1, Y=%2 cm")
            .arg(pos.x, 0, 'f', 1)
            .arg(pos.y, 0, 'f', 1), 3000);
    });
    connect(m_controlPanel, &RobotControlPanel::pathPlanned,
            m_coordinationCanvas, &CoordinationCanvas::showPath);

    connect(Logger::instance(), &Logger::newLogEntry, m_logModel, &LogModel::appendEntry);
}

QString MainWindow::robotLabel(const RobotId &id) const {
    auto *robot = m_robotManager->robot(id);
    if (!robot) return id;
    const QString port = robot->deviceInfo().portName;
    return robot->name().isEmpty() || robot->name() == port ? port
        : QString("%1 (%2)").arg(robot->name(), port);
}

// ---- Settings and persistence ----
void MainWindow::applySettings() {
    if (!m_settings) return;
    m_serialManager->setAutoReconnect(m_settings->autoReconnect(), m_settings->reconnectAttempts());
    m_serialManager->setDefaultPollInterval(m_settings->sensorPollIntervalMs());
    m_controlPanel->setSpeedLimit(m_settings->speedLimit());
    setKeyboardDriveEnabled(m_settings->keyboardDriveEnabled());
}

void MainWindow::showSettingsDialog() {
    if (!m_settings) return;
    releaseDriveKeys();
    SettingsDialog dialog(m_settings, this);
    if (dialog.exec() == QDialog::Accepted) applySettings();
}

void MainWindow::loadKnownDevices() {
    if (!m_settings) return;
    for (const auto &device : m_settings->knownDevices()) {
        BLEDeviceInfo info;
        info.portName = device.portName;
        info.friendlyName = device.name.isEmpty() ? device.portName : device.name;
        info.baudRate = device.baudRate;
        info.isEpuckMini = true;
        auto *robot = m_robotManager->addRobot(info);
        robot->setSerialManager(m_serialManager);
        m_serialManager->rememberPort(device.portName, device.baudRate);
    }
}

void MainWindow::saveKnownDevices() {
    if (!m_settings) return;
    QList<Settings::KnownDevice> devices;
    for (auto *robot : m_robotManager->allRobots()) {
        const auto &info = robot->deviceInfo();
        if (info.portName.isEmpty()) continue;
        Settings::KnownDevice device;
        device.portName = info.portName;
        device.name = robot->name();
        device.baudRate = info.baudRate;
        device.wasConnected = robot->state() == RobotConnectionState::Connected
            || m_serialManager->isReconnectPending(robot->id());
        devices.append(device);
    }
    m_settings->setKnownDevices(devices);
}

void MainWindow::saveWindowState() {
    if (!m_settings) return;
    m_settings->setMainWindowGeometry(saveGeometry());
    m_settings->setMainWindowState(saveState(kLayoutVersion));
    m_settings->setLastControlMode(m_controlMode);
    m_settings->setSpeedLimit(m_controlPanel->speedLimit());
    m_settings->setKeyboardDriveEnabled(m_keyboardDrive);
}

void MainWindow::restoreWindowState() {
    int mode = 0;
    if (m_settings) {
        const QByteArray geo = m_settings->mainWindowGeometry();
        if (!geo.isEmpty()) restoreGeometry(geo);
        const QByteArray state = m_settings->mainWindowState();
        if (!state.isEmpty()) restoreState(state, kLayoutVersion);
        mode = m_settings->lastControlMode();
    }
    // The mode decides where the control dock lives, so it is applied after any
    // restored layout rather than trusting the saved dock positions.
    setControlMode(mode);
}

void MainWindow::showConnectDialog() {
    releaseDriveKeys();
    ConnectDialog dialog(m_serialManager, this);
    if (dialog.exec() != QDialog::Accepted) return;
    const BLEDeviceInfo info = dialog.selectedDevice();
    if (info.portName.isEmpty()) {
        statusBar()->showMessage("错误：无效的端口名", 5000);
        return;
    }
    if (m_serialManager->connectToPort(info.portName, info.deriveId(), info.baudRate)) {
        statusBar()->showMessage(QString("正在后台连接 %1…").arg(info.portName), 5000);
    } else {
        statusBar()->showMessage(QString("端口 %1 连接请求失败").arg(info.portName), 5000);
    }
}

bool MainWindow::requestRobotConnection(const QString &id) {
    auto *robot = m_robotManager->robot(id);
    if (!robot) return false;

    const auto &info = robot->deviceInfo();
    if (info.portName.isEmpty()) {
        statusBar()->showMessage("错误：无效的端口名", 5000);
        return false;
    }

    statusBar()->showMessage(QString("正在后台连接 %1…").arg(info.portName));
    if (!m_serialManager->connectToPort(info.portName, id, info.baudRate)) {
        statusBar()->showMessage(QString("端口 %1 连接请求失败").arg(info.portName), 5000);
        return false;
    }
    return true;
}

void MainWindow::closeEvent(QCloseEvent *event) {
    releaseDriveKeys();
    m_controlPanel->stopManualControls();
    m_coordinator->emergencyStop();
    // Saved before the links are torn down so "was connected" reflects this session.
    saveKnownDevices();
    saveWindowState();
    event->accept();
}

void MainWindow::setControlMode(int mode) {
    releaseDriveKeys();
    m_controlMode = mode == 1 ? 1 : 0;
    m_controlPanel->setControlMode(m_controlMode);
    m_singleModeAction->setChecked(m_controlMode == 0);
    m_multiModeAction->setChecked(m_controlMode == 1);
    m_sensorDisplayWidget->setMultiRobotMode(m_controlMode == 1);
    removeDockWidget(m_controlPanelDock);
    m_controlPanelDock->setMinimumWidth(m_controlMode == 1 ? 0 : 330);
    refreshControlDockTitle();
    if (m_controlMode == 1) {
        addDockWidget(Qt::BottomDockWidgetArea, m_controlPanelDock);
        resizeDocks({m_controlPanelDock}, {340}, Qt::Vertical);
    } else {
        addDockWidget(Qt::RightDockWidgetArea, m_controlPanelDock);
        splitDockWidget(m_sensorDisplayDock, m_controlPanelDock, Qt::Vertical);
        resizeDocks({m_sensorDisplayDock, m_controlPanelDock}, {300, 460}, Qt::Vertical);
    }
    m_controlPanelDock->show();
    resizeDocks({m_robotListDock}, {230}, Qt::Horizontal);
    resizeDocks({m_sensorDisplayDock}, {350}, Qt::Horizontal);
    QTimer::singleShot(0, m_coordinationCanvas, &CoordinationCanvas::fitAllRobots);
}

void MainWindow::refreshControlDockTitle() {
    m_controlPanelDock->setWindowTitle(m_controlMode == 1 ? QString("多机 · 同步与分控")
        : m_singleSummary.isEmpty() ? QString("单机 · 驾驶与任务")
        : QString("单机 · %1").arg(m_singleSummary));
}

void MainWindow::emergencyStop() {
    releaseDriveKeys();
    m_controlPanel->stopManualControls();
    m_coordinator->emergencyStop();
    statusBar()->showMessage("紧急停止 - 已向所有机器人发送停止指令", 5000);
}

void MainWindow::refreshStatus() {
    const int online = m_robotManager->connectedRobots().size();
    const int total = m_robotManager->robotCount();
    setChip(m_onlineLabel, QString("在线 %1 / %2").arg(online).arg(total), online > 0 ? "ok" : "");
    if (m_coordinator->isPaused()) setChip(m_coordinationLabel, "协调已暂停", "warn");
    else if (m_coordinator->isActive()) setChip(m_coordinationLabel, "协调运行中", "info");
    else setChip(m_coordinationLabel, "手动控制", "");
    setChip(m_keyboardLabel, m_keyboardDrive ? (m_driveKeys.isEmpty() ? "键盘驾驶 开" : "键盘驾驶中")
                                             : "键盘驾驶 关",
            !m_keyboardDrive ? "" : m_driveKeys.isEmpty() ? "ok" : "info");
    m_disconnectAction->setEnabled(m_serialManager->isRobotConnected(m_robotManager->selectedRobotId()));
}

// ---- Keyboard driving ----
void MainWindow::setKeyboardDriveEnabled(bool enabled) {
    if (!enabled) releaseDriveKeys();
    m_keyboardDrive = enabled;
    if (m_keyboardAction && m_keyboardAction->isChecked() != enabled) m_keyboardAction->setChecked(enabled);
    if (m_keyboardLabel) refreshStatus();
}

void MainWindow::releaseDriveKeys() {
    if (m_driveKeys.isEmpty()) return;
    m_driveKeys.clear();
    if (m_controlPanel) m_controlPanel->keyboardDrive(0, 0);
}

void MainWindow::updateKeyboardDrive() {
    const double forward = (m_driveKeys.contains(Qt::Key_W) ? 1 : 0) - (m_driveKeys.contains(Qt::Key_S) ? 1 : 0);
    const double turn = (m_driveKeys.contains(Qt::Key_D) ? 1 : 0) - (m_driveKeys.contains(Qt::Key_A) ? 1 : 0);
    m_controlPanel->keyboardDrive(forward, turn);
}

bool MainWindow::handleDriveKey(int key, bool pressed, bool autoRepeat) {
    if (key == Qt::Key_Space) {
        // Space always stops, and is swallowed so it cannot also click a focused button.
        if (pressed && !autoRepeat) {
            m_driveKeys.clear();
            m_controlPanel->stopManualControls();
            statusBar()->showMessage("已停止手动驾驶", 2000);
        }
        return true;
    }
    if (key != Qt::Key_W && key != Qt::Key_A && key != Qt::Key_S && key != Qt::Key_D) return false;
    if (autoRepeat) return true;   // the held state is already tracked
    if (pressed) {
        if (!m_controlPanel->canKeyboardDrive()) return false;
        m_driveKeys.insert(key);
    } else if (!m_driveKeys.remove(key)) {
        return false;
    }
    updateKeyboardDrive();
    return true;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    const auto type = event->type();
    if (type == QEvent::KeyRelease) {
        // The release of a held drive key is honoured unconditionally: whatever has
        // happened since the press (a menu or combo popup opened, a dialog appeared,
        // focus or modifiers changed), the key must never stay "held".
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (!keyEvent->isAutoRepeat() && m_driveKeys.contains(keyEvent->key())) {
            handleDriveKey(keyEvent->key(), false, false);
        }
    } else if (type == QEvent::KeyPress && m_keyboardDrive
               && QApplication::activeWindow() == this
               && !QApplication::activeModalWidget() && !QApplication::activePopupWidget()) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        const auto blocking = Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
        if (!isTextEntry(QApplication::focusWidget()) && !(keyEvent->modifiers() & blocking)
            && handleDriveKey(keyEvent->key(), true, keyEvent->isAutoRepeat())) {
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::event(QEvent *event) {
    // WindowDeactivate is delivered to event(), never to changeEvent(), so this is
    // the place to guarantee that a window in the background cannot keep driving.
    if (event->type() == QEvent::WindowDeactivate && m_controlPanel) {
        m_driveKeys.clear();
        m_controlPanel->stopManualControls();
    }
    return QMainWindow::event(event);
}
