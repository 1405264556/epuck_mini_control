#include "RobotControlPanel.h"
#include "MotorControlWidget.h"
#include "LEDControlWidget.h"
#include "MultiRobotDriveWidget.h"
#include "core/RobotManager.h"
#include "core/RobotInstance.h"
#include "control/MultiRobotCoordinator.h"
#include "control/PathPlanner.h"
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

RobotControlPanel::RobotControlPanel(RobotManager *robotMgr,
                                     MultiRobotCoordinator *coordinator,
                                     QWidget *parent)
    : QWidget(parent)
    , m_robotManager(robotMgr)
    , m_coordinator(coordinator)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_modeCombo = new QComboBox;
    m_modeCombo->addItem("单机器人控制");
    m_modeCombo->addItem("多机器人控制");
    layout->addWidget(m_modeCombo);

    m_modeStack = new QStackedWidget;
    m_modeStack->addWidget(createSingleRobotPage());
    m_modeStack->addWidget(createMultiRobotPage());
    layout->addWidget(m_modeStack);

    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            m_modeStack, &QStackedWidget::setCurrentIndex);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RobotControlPanel::refreshRobotSummary);
    connect(m_robotManager, &RobotManager::selectedRobotChanged,
            this, &RobotControlPanel::onSelectionChanged);
    connect(m_robotManager, &RobotManager::robotConnectionStateChanged,
            this, [this](const RobotId &, RobotConnectionState) { refreshRobotSummary(); });
    connect(m_robotManager, &RobotManager::robotAdded,
            this, [this](const RobotId &) { refreshRobotSummary(); });
    connect(m_robotManager, &RobotManager::robotRemoved,
            this, [this](const RobotId &) { refreshRobotSummary(); });

    refreshRobotSummary();
}

QWidget *RobotControlPanel::createSingleRobotPage() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    m_singleSummary = new QLabel("未选择机器人");
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

    auto *pathBtns = new QHBoxLayout;
    auto *pickGoalBtn = new QPushButton("地图选点");
    auto *obstacleBtn = new QPushButton("添加障碍");
    auto *planBtn = new QPushButton("规划路径");
    m_startPathBtn = new QPushButton("开始跟踪");
    m_startPathBtn->setEnabled(false);
    pathBtns->addWidget(pickGoalBtn);
    pathBtns->addWidget(obstacleBtn);
    pathBtns->addWidget(planBtn);
    pathBtns->addWidget(m_startPathBtn);
    pathLayout->addLayout(pathBtns);

    auto *runBtns = new QHBoxLayout;
    auto *pauseBtn = new QPushButton("暂停");
    auto *resumeBtn = new QPushButton("继续");
    auto *returnBtn = new QPushButton("回原点");
    auto *clearBtn = new QPushButton("清理画布");
    runBtns->addWidget(pauseBtn);
    runBtns->addWidget(resumeBtn);
    runBtns->addWidget(returnBtn);
    runBtns->addWidget(clearBtn);
    pathLayout->addLayout(runBtns);

    m_pathStatus = new QLabel("可输入目标坐标，也可在中央画布选择目标、添加虚拟障碍。");
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
    m_singleTabs->addTab(pathTab, "路径规划");

    layout->addWidget(m_singleTabs);
    return page;
}

QWidget *RobotControlPanel::createMultiRobotPage() {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    m_multiSummary = new QLabel;
    m_multiSummary->setWordWrap(true);
    layout->addWidget(m_multiSummary);

    auto *tabs = new QTabWidget;
    m_multiDriveWidget = new MultiRobotDriveWidget(m_robotManager);
    tabs->addTab(m_multiDriveWidget, "同步与分控");

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
    tabs->addTab(coordinationPage, "协调算法");
    layout->addWidget(tabs, 1);

    connect(startFormationBtn, &QPushButton::clicked, this, &RobotControlPanel::startFormationControl);
    connect(pauseBtn, &QPushButton::clicked, this, &RobotControlPanel::pausePathFollowing);
    connect(resumeBtn, &QPushButton::clicked, this, &RobotControlPanel::resumePathFollowing);
    connect(stopBtn, &QPushButton::clicked, this, &RobotControlPanel::stopCoordination);
    return page;
}

void RobotControlPanel::onSelectionChanged(const RobotId &id) {
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

QVector<Vec2> RobotControlPanel::planWithBuiltInAStar(const Vec2 &start, const Vec2 &goal) const {
    constexpr double mapHalfRange = 200.0;
    PathPlanner planner;
    planner.setGridSize(static_cast<int>(mapHalfRange * 2), static_cast<int>(mapHalfRange * 2), 1.0);

    QVector<Vec2> shiftedObstacles;
    shiftedObstacles.reserve(m_virtualObstacles.size());
    for (const auto &obstacle : m_virtualObstacles) {
        shiftedObstacles.append(obstacle + Vec2(mapHalfRange, mapHalfRange));
    }
    planner.setObstacles(shiftedObstacles, 8.0);

    auto path = planner.smoothPath(
        planner.findPath(start + Vec2(mapHalfRange, mapHalfRange),
                         goal + Vec2(mapHalfRange, mapHalfRange)),
        0.35);
    for (auto &point : path) {
        point -= Vec2(mapHalfRange, mapHalfRange);
    }
    return path;
}

QVector<Vec2> RobotControlPanel::planWithExternalScript(const Vec2 &start, const Vec2 &goal) const {
    const QString scriptPath = m_scriptPathEdit->text().trimmed();
    if (scriptPath.isEmpty()) {
        const_cast<QLabel *>(m_pathStatus)->setText("请先选择 Python 或 MATLAB 路径规划脚本。");
        return {};
    }

    QJsonObject root;
    root["start"] = QJsonArray{start.x, start.y};
    root["goal"] = QJsonArray{goal.x, goal.y};
    QJsonArray obstacles;
    for (const auto &obstacle : m_virtualObstacles) {
        obstacles.append(QJsonArray{obstacle.x, obstacle.y});
    }
    root["obstacles"] = obstacles;
    root["bounds"] = QJsonArray{-200.0, 200.0, -200.0, 200.0};
    root["obstacle_radius"] = 8.0;

    QTemporaryFile inputFile(QDir::tempPath() + "/epuck_planner_XXXXXX.json");
    if (!inputFile.open()) {
        const_cast<QLabel *>(m_pathStatus)->setText("无法创建路径规划临时输入文件。");
        return {};
    }
    inputFile.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    inputFile.flush();

    const QString mode = m_algorithmCombo->currentData().toString();
    QString program;
    QStringList args;
    if (mode == "python") {
        program = "python";
        args << scriptPath << inputFile.fileName();
    } else {
        program = "matlab";
        const QFileInfo info(scriptPath);
        const QString functionName = info.completeBaseName();
        const QString batch = QString(
            "addpath('%1'); data=jsondecode(fileread('%2')); path=%3(data); disp(jsonencode(path));")
            .arg(QDir::toNativeSeparators(info.absolutePath()).replace("\\", "/"),
                 QDir::toNativeSeparators(inputFile.fileName()).replace("\\", "/"),
                 functionName);
        args << "-batch" << batch;
    }

    QProcess process;
    process.start(program, args);
    if (!process.waitForFinished(30000)) {
        process.kill();
        const_cast<QLabel *>(m_pathStatus)->setText("外部路径规划脚本超时。");
        return {};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString err = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        const_cast<QLabel *>(m_pathStatus)->setText(QString("外部路径规划失败：%1").arg(err.left(160)));
        return {};
    }

    const QByteArray output = process.readAllStandardOutput().trimmed();
    const int firstBracket = output.indexOf('[');
    const int lastBracket = output.lastIndexOf(']');
    if (firstBracket < 0 || lastBracket <= firstBracket) {
        const_cast<QLabel *>(m_pathStatus)->setText("外部脚本未输出路径 JSON 数组。");
        return {};
    }
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(output.mid(firstBracket, lastBracket - firstBracket + 1), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        const_cast<QLabel *>(m_pathStatus)->setText(QString("路径 JSON 解析失败：%1").arg(parseError.errorString()));
        return {};
    }

    QVector<Vec2> path;
    for (const auto &value : doc.array()) {
        const auto point = value.toArray();
        if (point.size() >= 2) {
            path.append(Vec2(point.at(0).toDouble(), point.at(1).toDouble()));
        }
    }
    return path;
}

void RobotControlPanel::planSingleRobotPath() {
    auto *robot = m_robotManager->robot(m_currentId);
    if (!robot || robot->state() != RobotConnectionState::Connected) {
        m_pathStatus->setText("请先选择并连接一台机器人。");
        m_startPathBtn->setEnabled(false);
        return;
    }

    const Vec2 start = robot->position();
    const Vec2 goal(m_goalXSpin->value(), m_goalYSpin->value());
    const QString mode = m_algorithmCombo->currentData().toString();
    m_lastPlannedPath = (mode == "builtin")
        ? planWithBuiltInAStar(start, goal)
        : planWithExternalScript(start, goal);

    if (m_lastPlannedPath.isEmpty()) {
        m_pathStatus->setText("未找到可用路径，请调整目标点、障碍物或算法脚本。");
        m_startPathBtn->setEnabled(false);
        return;
    }

    m_pathStatus->setText(QString("路径规划完成，共 %1 个路径点，障碍物 %2 个。")
                          .arg(m_lastPlannedPath.size())
                          .arg(m_virtualObstacles.size()));
    m_startPathBtn->setEnabled(true);
    emit pathPlanned(m_currentId, m_lastPlannedPath);
}

void RobotControlPanel::startSingleRobotPath() {
    if (m_currentId.isEmpty() || m_lastPlannedPath.isEmpty() || !m_coordinator) return;
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
    planSingleRobotPath();
    startSingleRobotPath();
    m_pathStatus->setText("已规划并启动回原点路径。");
}

void RobotControlPanel::clearCanvas() {
    m_lastPlannedPath.clear();
    if (m_startPathBtn) m_startPathBtn->setEnabled(false);
    emit canvasClearRequested();
    m_pathStatus->setText("画布路径、轨迹和虚拟障碍已清理。");
}

void RobotControlPanel::setGoalFromCanvas(const Vec2 &pos) {
    m_goalXSpin->setValue(pos.x);
    m_goalYSpin->setValue(pos.y);
    m_pathStatus->setText(QString("已选择目标点：X=%1 cm, Y=%2 cm。")
                          .arg(pos.x, 0, 'f', 1)
                          .arg(pos.y, 0, 'f', 1));
    planSingleRobotPath();
}

void RobotControlPanel::setVirtualObstacles(const QVector<Vec2> &obstacles) {
    m_virtualObstacles = obstacles;
    m_pathStatus->setText(QString("已设置 %1 个虚拟障碍。").arg(m_virtualObstacles.size()));
}

void RobotControlPanel::startFormationControl() {
    const auto ids = connectedRobotIds();
    if (ids.size() < 2) {
        m_multiSummary->setText("编队控制至少需要 2 台已连接机器人。");
        return;
    }

    if (!m_coordinator) return;
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

void RobotControlPanel::stopCoordination() {
    if (m_coordinator) {
        m_coordinator->stopAll();
    }
    refreshRobotSummary();
}
