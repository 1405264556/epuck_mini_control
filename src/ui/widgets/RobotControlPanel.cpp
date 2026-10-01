#include "RobotControlPanel.h"
#include "MotorControlWidget.h"
#include "LEDControlWidget.h"
#include "MultiRobotDriveWidget.h"
#include "core/RobotManager.h"
#include "core/RobotInstance.h"
#include "control/MultiRobotCoordinator.h"
#include "control/PathPlanner.h"
#include "control/PathPlanningJob.h"
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTemporaryFile>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QStyle>
#include <QGridLayout>

RobotControlPanel::RobotControlPanel(RobotManager *robotMgr,
                                     MultiRobotCoordinator *coordinator,
                                     QWidget *parent)
    : QWidget(parent)
    , m_robotManager(robotMgr)
    , m_coordinator(coordinator)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_modeStack = new QStackedWidget;
    m_modeStack->addWidget(createSingleRobotPage());
    m_modeStack->addWidget(createMultiRobotPage());
    layout->addWidget(m_modeStack, 1);
    connect(m_motorWidget, &MotorControlWidget::manualCommandRequested, this,
            [this](const RobotId &id, double left, double right) {
        if (m_coordinator) m_coordinator->manualDrive({{id, {left, right}}});
    });
    if (m_coordinator) connect(m_coordinator, &MultiRobotCoordinator::coordinationStarted,
        this, [this]() { m_motorWidget->releaseControl(false); });
    connect(m_robotManager, &RobotManager::selectedRobotChanged,
            this, &RobotControlPanel::onSelectionChanged);
    connect(m_robotManager, &RobotManager::robotConnectionStateChanged,
            this, [this](const RobotId &, RobotConnectionState) { refreshRobotSummary(); });
    connect(m_robotManager, &RobotManager::robotAdded,
            this, [this](const RobotId &) { refreshRobotSummary(); });
    connect(m_robotManager, &RobotManager::robotRemoved,
            this, [this](const RobotId &) { refreshRobotSummary(); });

    onSelectionChanged(m_robotManager->selectedRobotId());
}

QWidget *RobotControlPanel::createSingleRobotPage() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    layout->setContentsMargins(4, 4, 4, 4);
    layout->setContentsMargins(4, 4, 4, 4);

    m_singleSummary = new QLabel("未选择机器人");
    m_singleSummary->setWordWrap(true);
    layout->addWidget(m_singleSummary);

    m_singleTabs = new QTabWidget;
    m_motorWidget = new MotorControlWidget(m_robotManager);
    m_ledWidget = new LEDControlWidget(m_robotManager);
    m_singleTabs->addTab(m_motorWidget, "驱动");
    m_singleTabs->addTab(m_ledWidget, "LED 灯");

    const double planningRange = 200.0;
    auto *pathTab = new QWidget;
    auto *pathLayout = new QVBoxLayout(pathTab);

    auto *goalBox = new QGroupBox("目标点");
    auto *goalForm = new QFormLayout(goalBox);
    m_goalXSpin = new QDoubleSpinBox;
    m_goalXSpin->setRange(-planningRange, planningRange);
    m_goalXSpin->setValue(100);
    m_goalXSpin->setSuffix(" cm");
    m_goalYSpin = new QDoubleSpinBox;
    m_goalYSpin->setRange(-planningRange, planningRange);
    m_goalYSpin->setValue(100);
    m_goalYSpin->setSuffix(" cm");
    goalForm->addRow("目标 X:", m_goalXSpin);
    goalForm->addRow("目标 Y:", m_goalYSpin);
    auto invalidateGoal = [this]() {
        if (m_planningJob) { delete m_planningJob; m_planningJob = nullptr; }
        m_lastPlannedPath.clear();
        if (m_startPathBtn) m_startPathBtn->setEnabled(false);
    };
    connect(m_goalXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, invalidateGoal);
    connect(m_goalYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, invalidateGoal);
    pathLayout->addWidget(goalBox);

    auto *algoBox = new QGroupBox("规划算法");
    auto *algoLayout = new QVBoxLayout(algoBox);
    auto *algoForm = new QFormLayout;
    m_algorithmCombo = new QComboBox;
    m_algorithmCombo->addItem("内置 A*", "builtin");
    m_algorithmCombo->addItem("Python 脚本", "python");
    m_algorithmCombo->addItem("MATLAB 脚本", "matlab");
    algoForm->addRow("算法:", m_algorithmCombo);
    algoLayout->addLayout(algoForm);

    auto *scriptRow = new QHBoxLayout;
    m_scriptPathEdit = new QLineEdit;
    m_scriptPathEdit->setPlaceholderText("Python/MATLAB 脚本路径");
    auto *browseBtn = new QPushButton("浏览");
    scriptRow->addWidget(m_scriptPathEdit);
    scriptRow->addWidget(browseBtn);
    algoLayout->addLayout(scriptRow);
    pathLayout->addWidget(algoBox);

    auto *pathBtns = new QGridLayout;
    auto *pickGoalBtn = new QPushButton("地图选点");
    auto *obstacleBtn = new QPushButton("添加障碍");
    auto *planBtn = new QPushButton("规划路径");
    m_startPathBtn = new QPushButton("开始跟踪");
    m_startPathBtn->setEnabled(false);
    pathBtns->addWidget(pickGoalBtn, 0, 0);
    pathBtns->addWidget(obstacleBtn, 0, 1);
    pathBtns->addWidget(planBtn, 1, 0);
    pathBtns->addWidget(m_startPathBtn, 1, 1);
    pathLayout->addLayout(pathBtns);

    auto *runBtns = new QGridLayout;
    auto *pauseBtn = new QPushButton("暂停");
    auto *resumeBtn = new QPushButton("继续");
    auto *returnBtn = new QPushButton("回原点");
    auto *clearBtn = new QPushButton("清理画布");
    runBtns->addWidget(pauseBtn, 0, 0);
    runBtns->addWidget(resumeBtn, 0, 1);
    runBtns->addWidget(returnBtn, 1, 0);
    runBtns->addWidget(clearBtn, 1, 1);
    pathLayout->addLayout(runBtns);

    m_pathStatus = new QLabel("未规划路径");
    m_pathStatus->setWordWrap(true);
    pathLayout->addWidget(m_pathStatus);
    pathLayout->addStretch();

    connect(browseBtn, &QPushButton::clicked, this, &RobotControlPanel::browsePlannerScript);
    connect(pickGoalBtn, &QPushButton::clicked, this, &RobotControlPanel::requestGoalPick);
    connect(obstacleBtn, &QPushButton::clicked, this, [this]() {
        emit obstacleEditRequested();
        m_pathStatus->setText("请在中央画布点击虚拟障碍位置，可连续添加。");
    });
    connect(planBtn, &QPushButton::clicked, this, &RobotControlPanel::planSingleRobotPath);
    connect(m_startPathBtn, &QPushButton::clicked, this, &RobotControlPanel::startSingleRobotPath);
    connect(pauseBtn, &QPushButton::clicked, this, &RobotControlPanel::pausePathFollowing);
    connect(resumeBtn, &QPushButton::clicked, this, &RobotControlPanel::resumePathFollowing);
    connect(returnBtn, &QPushButton::clicked, this, &RobotControlPanel::returnSingleRobotOrigin);
    connect(clearBtn, &QPushButton::clicked, this, &RobotControlPanel::clearCanvas);
    auto *pathScroll = new QScrollArea;
    pathScroll->setWidgetResizable(true);
    pathScroll->setFrameShape(QFrame::NoFrame);
    pathScroll->setWidget(pathTab);
    m_singleTabs->addTab(pathScroll, "路径规划");

    layout->addWidget(m_singleTabs);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(page);
    return scroll;
}

QWidget *RobotControlPanel::createMultiRobotPage() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    m_multiSummary = new QLabel;
    m_multiSummary->setWordWrap(true);
    layout->addWidget(m_multiSummary);

    auto *tabs = new QTabWidget;
    m_multiTabs = tabs;
    m_multiDriveWidget = new MultiRobotDriveWidget(m_robotManager, m_coordinator);
    auto *driveScroll = new QScrollArea;
    driveScroll->setWidgetResizable(true);
    driveScroll->setFrameShape(QFrame::NoFrame);
    driveScroll->setWidget(m_multiDriveWidget);
    tabs->addTab(driveScroll, "同步与分控");

    auto *coordinationPage = new QWidget;
    auto *coordinationLayout = new QVBoxLayout(coordinationPage);
    auto *formationBox = new QGroupBox("协调控制参数");
    auto *form = new QFormLayout(formationBox);
    m_coordinationModeCombo = new QComboBox;
    m_coordinationModeCombo->addItem("队形保持", "formation");
    m_coordinationModeCombo->addItem("群集目标跟随", "flocking");
    if (m_coordinator) {
        for (const auto &strategyId : m_coordinator->availableStrategyIds()) {
            m_coordinationModeCombo->addItem(
                m_coordinator->strategyDisplayName(strategyId), "plugin:" + strategyId);
        }
    }
    m_formationShapeCombo = new QComboBox;
    m_formationShapeCombo->addItem("横列", static_cast<int>(FormationShape::Line));
    m_formationShapeCombo->addItem("圆形", static_cast<int>(FormationShape::Circle));
    m_formationShapeCombo->addItem("V 字形", static_cast<int>(FormationShape::VShape));
    m_formationShapeCombo->addItem("楔形", static_cast<int>(FormationShape::Wedge));
    m_formationShapeCombo->addItem("纵队", static_cast<int>(FormationShape::Column));
    m_formationShapeCombo->addItem("菱形", static_cast<int>(FormationShape::Diamond));

    m_spacingSpin = new QDoubleSpinBox;
    m_spacingSpin->setRange(5, 100);
    m_spacingSpin->setValue(20);
    m_spacingSpin->setSingleStep(1);
    m_spacingSpin->setAccelerated(true);
    m_spacingSpin->setKeyboardTracking(false);
    m_spacingSpin->setSuffix(" cm");
    m_targetXSpin = new QDoubleSpinBox;
    m_targetXSpin->setRange(-200, 200);
    m_targetXSpin->setValue(120);
    m_targetXSpin->setSingleStep(5);
    m_targetXSpin->setAccelerated(true);
    m_targetXSpin->setKeyboardTracking(false);
    m_targetXSpin->setSuffix(" cm");
    m_targetYSpin = new QDoubleSpinBox;
    m_targetYSpin->setRange(-200, 200);
    m_targetYSpin->setValue(120);
    m_targetYSpin->setSingleStep(5);
    m_targetYSpin->setAccelerated(true);
    m_targetYSpin->setKeyboardTracking(false);
    m_targetYSpin->setSuffix(" cm");

    auto makeSliderControl = [](QDoubleSpinBox *spin, int minimum, int maximum) {
        auto *widget = new QWidget;
        auto *row = new QHBoxLayout(widget);
        row->setContentsMargins(0, 0, 0, 0);
        auto *slider = new QSlider(Qt::Horizontal);
        slider->setRange(minimum, maximum);
        slider->setValue(qRound(spin->value()));
        slider->setSingleStep(1);
        slider->setPageStep(10);
        row->addWidget(slider, 1);
        row->addWidget(spin);
        connect(slider, &QSlider::valueChanged, spin,
                [spin](int value) { spin->setValue(value); });
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), slider,
                [slider](double value) { slider->setValue(qRound(value)); });
        return widget;
    };

    form->addRow("控制模式:", m_coordinationModeCombo);
    form->addRow("队形:", m_formationShapeCombo);
    form->addRow("间距:", makeSliderControl(m_spacingSpin, 5, 100));
    form->addRow("目标 X:", makeSliderControl(m_targetXSpin, -200, 200));
    form->addRow("目标 Y:", makeSliderControl(m_targetYSpin, -200, 200));
    coordinationLayout->addWidget(formationBox);

    auto *btnRow = new QHBoxLayout;
    auto *startFormationBtn = new QPushButton("启动算法");
    auto *pauseBtn = new QPushButton("暂停");
    auto *resumeBtn = new QPushButton("继续");
    auto *stopBtn = new QPushButton("停止协同");
    btnRow->addWidget(startFormationBtn);
    btnRow->addWidget(pauseBtn);
    btnRow->addWidget(resumeBtn);
    btnRow->addWidget(stopBtn);
    coordinationLayout->addLayout(btnRow);
    coordinationLayout->addStretch();
    auto *coordinationScroll = new QScrollArea;
    coordinationScroll->setWidgetResizable(true);
    coordinationScroll->setFrameShape(QFrame::NoFrame);
    coordinationScroll->setWidget(coordinationPage);
    tabs->addTab(coordinationScroll, "协调算法");
    layout->addWidget(tabs, 1);

    connect(startFormationBtn, &QPushButton::clicked, this, &RobotControlPanel::startFormationControl);
    connect(pauseBtn, &QPushButton::clicked, this, &RobotControlPanel::pausePathFollowing);
    connect(resumeBtn, &QPushButton::clicked, this, &RobotControlPanel::resumePathFollowing);
    connect(stopBtn, &QPushButton::clicked, this, &RobotControlPanel::stopCoordination);
    return page;
}

void RobotControlPanel::onSelectionChanged(const RobotId &id) {
    if (id == m_currentId) { refreshRobotSummary(); return; }
    if (m_planningJob) { delete m_planningJob; m_planningJob = nullptr; }
    m_currentId = id;
    m_motorWidget->setCurrentRobot(id);
    m_ledWidget->setCurrentRobot(id);
    m_lastPlannedPath.clear();
    if (m_startPathBtn) m_startPathBtn->setEnabled(false);
    refreshRobotSummary();
}

QList<RobotId> RobotControlPanel::connectedRobotIds() const {
    QList<RobotId> ids;
    for (auto *robot : m_robotManager->connectedRobots()) {
        ids.append(robot->id());
    }
    return ids;
}

void RobotControlPanel::refreshRobotSummary() {
    auto *selected = m_robotManager->robot(m_currentId);
    if (selected) {
        const QString state = selected->state() == RobotConnectionState::Connected
            ? "已连接" : "未连接";
        m_singleSummary->setText(QString("当前机器人：%1（%2）").arg(selected->name(), state));
    } else {
        m_singleSummary->setText("未选择机器人");
    }

    const auto ids = connectedRobotIds();
    QStringList names;
    for (const auto &id : ids) {
        if (auto *robot = m_robotManager->robot(id)) names.append(robot->deviceInfo().portName);
    }
    m_multiSummary->setText(QString("在线：%1 台%2")
                            .arg(ids.size())
                            .arg(names.isEmpty() ? QString() : QString("（%1）").arg(names.join("、"))));
}

void RobotControlPanel::planSingleRobotPath() {
    const bool returnToOrigin = m_returnAfterPlan;
    m_returnAfterPlan = false;
    auto *robot = m_robotManager->robot(m_currentId);
    if (!robot || robot->state() != RobotConnectionState::Connected) {
        m_pathStatus->setText("请先连接机器人");
        return;
    }
    if (m_planningJob) { delete m_planningJob; m_planningJob = nullptr; }
    const RobotId plannedId = m_currentId;
    m_lastPlannedPath.clear();
    m_startPathBtn->setEnabled(false);
    m_pathStatus->setText("正在规划路径…");
    auto *job = new PathPlanningJob(this);
    m_planningJob = job;
    connect(job, &PathPlanningJob::finished, this,
            [this, job, plannedId, returnToOrigin](const QVector<Vec2> &path, const QString &error) {
        if (m_planningJob != job) return;
        m_planningJob = nullptr;
        job->deleteLater();
        if (plannedId != m_currentId) return;
        if (!error.isEmpty() || path.size() < 2) {
            m_pathStatus->setText(error.isEmpty() ? "未找到可用路径" : error);
            return;
        }
        m_lastPlannedPath = path;
        m_startPathBtn->setEnabled(true);
        m_pathStatus->setText(QString("路径已生成 · %1 个点").arg(path.size()));
        emit pathPlanned(plannedId, path);
        if (returnToOrigin) startSingleRobotPath();
    });
    job->start(robot->position(), {m_goalXSpin->value(), m_goalYSpin->value()},
               m_virtualObstacles, m_algorithmCombo->currentData().toString(), m_scriptPathEdit->text().trimmed());
}

void RobotControlPanel::startSingleRobotPath() {
    if (m_currentId.isEmpty() || m_lastPlannedPath.isEmpty() || !m_coordinator) return;
    stopManualControls();
    QMap<RobotId, QVector<Vec2>> paths;
    paths[m_currentId] = m_lastPlannedPath;
    m_coordinator->startPathFollowing(paths);
    m_pathStatus->setText("路径跟踪已启动。");
}

void RobotControlPanel::requestGoalPick() {
    emit goalPickRequested();
    m_pathStatus->setText("请在中央画布上点击目标位置。");
}

void RobotControlPanel::browsePlannerScript() {
    const QString path = QFileDialog::getOpenFileName(
        this, "选择路径规划脚本", QString(),
        "Planner scripts (*.py *.m);;All files (*.*)");
    if (!path.isEmpty()) {
        m_scriptPathEdit->setText(path);
        if (path.endsWith(".py", Qt::CaseInsensitive)) {
            m_algorithmCombo->setCurrentIndex(m_algorithmCombo->findData("python"));
        } else if (path.endsWith(".m", Qt::CaseInsensitive)) {
            m_algorithmCombo->setCurrentIndex(m_algorithmCombo->findData("matlab"));
        }
    }
}

void RobotControlPanel::pausePathFollowing() {
    if (!m_coordinator) return;
    m_coordinator->pause();
    m_pathStatus->setText("路径跟踪已暂停。");
}

void RobotControlPanel::resumePathFollowing() {
    if (!m_coordinator) return;
    m_coordinator->resume();
    m_pathStatus->setText("路径跟踪已继续。");
}

void RobotControlPanel::returnSingleRobotOrigin() {
    m_goalXSpin->setValue(0);
    m_goalYSpin->setValue(0);
    m_returnAfterPlan = true;
    planSingleRobotPath();
}

void RobotControlPanel::clearCanvas() {
    if (m_planningJob) { delete m_planningJob; m_planningJob = nullptr; }
    m_lastPlannedPath.clear();
    if (m_startPathBtn) m_startPathBtn->setEnabled(false);
    emit canvasClearRequested();
    m_pathStatus->setText("画布路径、轨迹和虚拟障碍已清理。");
}

void RobotControlPanel::setGoalFromCanvas(const Vec2 &pos) {
    if (m_modeStack->currentIndex() == 1) {
        m_targetXSpin->setValue(pos.x);
        m_targetYSpin->setValue(pos.y);
        return;
    }
    m_goalXSpin->setValue(pos.x);
    m_goalYSpin->setValue(pos.y);
    m_pathStatus->setText(QString("已选择目标点：X=%1 cm, Y=%2 cm。")
                          .arg(pos.x, 0, 'f', 1)
                          .arg(pos.y, 0, 'f', 1));
    planSingleRobotPath();
}

void RobotControlPanel::setVirtualObstacles(const QVector<Vec2> &obstacles) {
    if (m_planningJob) { delete m_planningJob; m_planningJob = nullptr; }
    m_lastPlannedPath.clear();
    m_startPathBtn->setEnabled(false);
    m_virtualObstacles = obstacles;
    m_pathStatus->setText(QString("已设置 %1 个虚拟障碍。").arg(m_virtualObstacles.size()));
}

void RobotControlPanel::startFormationControl() {
    const auto ids = m_multiDriveWidget->selectedRobotIds();
    if (ids.size() < 2) {
        m_multiSummary->setText("编队控制至少需要 2 台已连接机器人。");
        return;
    }

    if (!m_coordinator) return;
    stopManualControls();
    const Vec2 target(m_targetXSpin->value(), m_targetYSpin->value());
    const QString mode = m_coordinationModeCombo->currentData().toString();
    if (mode == "flocking") {
        m_coordinator->startFlocking(ids, target);
    } else if (mode.startsWith("plugin:")) {
        QVariantMap parameters;
        parameters["goal_x"] = target.x;
        parameters["goal_y"] = target.y;
        parameters["spacing_cm"] = m_spacingSpin->value();
        parameters["formation_shape"] = m_formationShapeCombo->currentData();
        m_coordinator->startStrategy(mode.mid(7), ids, parameters);
    } else {
        const auto shape = static_cast<FormationShape>(m_formationShapeCombo->currentData().toInt());
        m_coordinator->startFormation(shape, ids, m_spacingSpin->value(), target);
    }
    refreshRobotSummary();
}

void RobotControlPanel::setControlMode(int mode) {
    stopManualControls();
    m_modeStack->setCurrentIndex(mode == 1 ? 1 : 0);
    refreshRobotSummary();
}

void RobotControlPanel::showCoordinationPage() { m_multiTabs->setCurrentIndex(1); }
void RobotControlPanel::stopManualControls() {
    m_motorWidget->releaseControl();
    m_multiDriveWidget->stopManualControl();
}

void RobotControlPanel::stopCoordination() {
    if (m_coordinator) {
        m_coordinator->stopAll();
    }
    refreshRobotSummary();
}
