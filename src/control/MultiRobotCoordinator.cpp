#include "MultiRobotCoordinator.h"
#include "FormationController.h"
#include "FlockingController.h"
#include "PathPlanner.h"
#include "CollisionAvoidance.h"
#include "MultiRobotStrategyRegistry.h"
#include "core/RobotManager.h"
#include "core/RobotInstance.h"
#include "util/MathUtils.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>

MultiRobotCoordinator::MultiRobotCoordinator(RobotManager *robotManager, QObject *parent)
    : QObject(parent)
    , m_robotManager(robotManager)
    , m_formationController(new FormationController(this))
    , m_flockingController(new FlockingController(this))
    , m_pathPlanner(new PathPlanner(this))
    , m_strategyRegistry(new MultiRobotStrategyRegistry(this))
    , m_controlTimer(new QTimer(this))
{
    m_controlTimer->setInterval(50); // 20 Hz
    m_controlTimer->setTimerType(Qt::PreciseTimer);
    connect(m_controlTimer, &QTimer::timeout, this, &MultiRobotCoordinator::controlLoop);
    connect(m_flockingController, &FlockingController::velocitiesComputed, this,
            [this](const QMap<RobotId, WheelSpeeds> &commands) {
        for (auto it = commands.cbegin(); it != commands.cend(); ++it) {
            sendMotorCommand(it.key(), it.value().left, it.value().right);
        }
    });
    connect(m_strategyRegistry, &MultiRobotStrategyRegistry::strategiesChanged,
            this, &MultiRobotCoordinator::strategyPluginsChanged);
    reloadStrategyPlugins();
}

void MultiRobotCoordinator::startFormation(FormationShape shape, const QList<RobotId> &robotIds,
                                            double spacing, const Vec2 &targetPos) {
    stopAll();

    m_formationController->setShape(shape);
    m_formationController->setSpacing(spacing);
    m_formationController->setTargetPosition(targetPos);

    QList<RobotInstance *> robots;
    for (const auto &id : robotIds) {
        if (auto *r = m_robotManager->robot(id)) {
            if (r->state() == RobotConnectionState::Connected) robots.append(r);
        }
    }
    if (robots.isEmpty()) return;

    m_formationController->start(robots);
    m_active = true;
    m_paused = false;
    m_controlTimer->start();
    emit coordinationStarted();
}

void MultiRobotCoordinator::startFlocking(const QList<RobotId> &robotIds, const Vec2 &goal) {
    stopAll();

    m_flockingController->setGoalPosition(goal);
    m_flockingController->setMaxSpeed(10.0);

    QList<RobotInstance *> robots;
    for (const auto &id : robotIds) {
        if (auto *r = m_robotManager->robot(id)) {
            if (r->state() == RobotConnectionState::Connected) robots.append(r);
        }
    }
    if (robots.isEmpty()) return;

    m_flockingController->start(robots);
    m_active = true;
    m_paused = false;
    m_controlTimer->start();
    emit coordinationStarted();
}

void MultiRobotCoordinator::startPathFollowing(const QMap<RobotId, QVector<Vec2>> &robotPaths) {
    stopAll();
    m_activePaths = robotPaths;
    m_pathProgress.clear();
    for (auto it = robotPaths.begin(); it != robotPaths.end(); ++it) {
        m_pathProgress[it.key()] = 0;
    }
    m_active = true;
    m_paused = false;
    m_controlTimer->start();
    emit coordinationStarted();
}

bool MultiRobotCoordinator::startStrategy(const QString &strategyId,
                                          const QList<RobotId> &robotIds,
                                          const QVariantMap &parameters) {
    auto *strategy = m_strategyRegistry->strategy(strategyId);
    if (!strategy) {
        emit strategyError(QString("未找到协调算法插件：%1").arg(strategyId));
        return false;
    }

    stopAll();
    m_strategyRobotIds.clear();
    for (const auto &id : robotIds) {
        auto *robot = m_robotManager->robot(id);
        if (robot && robot->state() == RobotConnectionState::Connected) {
            m_strategyRobotIds.append(id);
        }
    }
    if (m_strategyRobotIds.isEmpty()) return false;

    m_strategyParameters = parameters;
    strategy->configure(parameters);
    strategy->reset(buildWorldState(0.05));
    m_activeStrategy = strategy;
    m_active = true;
    m_paused = false;
    m_controlTimer->start();
    emit coordinationStarted();
    return true;
}

int MultiRobotCoordinator::reloadStrategyPlugins(const QString &directoryPath) {
    if (m_activeStrategy) stopAll();
    const QString path = directoryPath.isEmpty()
        ? QDir(QCoreApplication::applicationDirPath()).filePath("algorithms")
        : directoryPath;
    return m_strategyRegistry->loadDirectory(path);
}

QStringList MultiRobotCoordinator::availableStrategyIds() const {
    QStringList ids;
    for (auto *strategy : m_strategyRegistry->strategies()) ids.append(strategy->id());
    return ids;
}

QString MultiRobotCoordinator::strategyDisplayName(const QString &strategyId) const {
    auto *strategy = m_strategyRegistry->strategy(strategyId);
    return strategy ? strategy->displayName() : strategyId;
}

void MultiRobotCoordinator::setNeighborTopology(
    const QMap<RobotId, QList<RobotId>> &topology) {
    m_neighborTopology = topology;
}

void MultiRobotCoordinator::pause() {
    if (!m_active || m_paused) return;
    m_paused = true;
    m_controlTimer->stop();
    for (auto *r : m_robotManager->allRobots()) {
        if (r->state() == RobotConnectionState::Connected) r->stop();
    }
    emit coordinationPaused();
}

void MultiRobotCoordinator::resume() {
    if (!m_active || !m_paused) return;
    m_paused = false;
    m_controlTimer->start();
    emit coordinationResumed();
}

void MultiRobotCoordinator::stopAll() {
    const bool wasActive = m_active;
    m_active = false;
    m_paused = false;
    m_controlTimer->stop();
    m_formationController->stop();
    m_flockingController->stop();
    if (m_activeStrategy) m_activeStrategy->stop();
    m_activeStrategy = nullptr;
    m_strategyRobotIds.clear();
    m_strategyParameters.clear();
    m_activePaths.clear();
    m_pathProgress.clear();
    if (wasActive) {
        for (auto *robot : m_robotManager->connectedRobots()) robot->stop();
        emit coordinationStopped();
    }
}

void MultiRobotCoordinator::emergencyStop() {
    stopAll();
    for (auto *r : m_robotManager->allRobots()) r->stop();
}

bool MultiRobotCoordinator::isActive() const { return m_active; }

void MultiRobotCoordinator::controlLoop() {
    if (!m_active) return;

    if (m_activeStrategy) {
        QElapsedTimer timer;
        timer.start();
        const auto output = m_activeStrategy->step(buildWorldState(0.05));
        for (auto it = output.wheelSpeeds.cbegin(); it != output.wheelSpeeds.cend(); ++it) {
            if (!m_strategyRobotIds.contains(it.key())) continue;
            sendMotorCommand(it.key(), math::clamp(it.value().left, -1.0, 1.0),
                             math::clamp(it.value().right, -1.0, 1.0));
        }
        if (timer.elapsed() > 40) {
            emit strategyError(QString("算法 %1 单周期耗时 %2 ms，接近 50 ms 控制周期")
                .arg(m_activeStrategy->displayName()).arg(timer.elapsed()));
        }
        return;
    }

    // Formation control
    if (m_formationController->isRunning()) {
        m_formationController->update(0.05);
        auto desiredPositions = m_formationController->desiredPositions();
        for (auto it = desiredPositions.begin(); it != desiredPositions.end(); ++it) {
            if (auto *r = m_robotManager->robot(it.key());
                r && r->state() == RobotConnectionState::Connected) {
                Vec2 pos = r->position();
                Vec2 target = it.value();

                double dist = pos.distanceTo(target);
                double heading = atan2(target.y - pos.y, target.x - pos.x);

                // Simple P controller for position tracking
                double linearVel = math::clamp(dist * 0.5, -10.0, 10.0);
                double headingError = math::normalizeAngle(heading - r->heading());
                double angularVel = math::clamp(headingError * 2.0, -4.0, 4.0);

                auto ws = math::unicycleToWheelSpeeds(linearVel, angularVel, 2.0, 5.0);
                sendMotorCommand(it.key(), ws.left, ws.right);
                r->setPosition(pos + Vec2(linearVel * 0.05 * cos(heading),
                                           linearVel * 0.05 * sin(heading)), heading);
            }
        }
    }

    // Flocking control
    if (m_flockingController->isRunning()) {
        m_flockingController->update(0.05);
    }

    // Path following
    QList<RobotId> completedPaths;
    for (auto it = m_activePaths.begin(); it != m_activePaths.end(); ++it) {
        const RobotId &id = it.key();
        auto *r = m_robotManager->robot(id);
        if (!r || r->state() != RobotConnectionState::Connected) continue;

        const auto &path = it.value();
        int progress = m_pathProgress.value(id, 0);
        if (path.size() < 2 || progress >= path.size() - 1) {
            r->stop();
            completedPaths.append(id);
            continue;
        }

        // Look-ahead: find next waypoint
        Vec2 pos = r->position();
        Vec2 target = path[progress + 1];
        double dist = pos.distanceTo(target);

        if (dist < 5.0 && progress + 2 < path.size()) {
            progress++;
            m_pathProgress[id] = progress;
            target = path[progress + 1];
        }

        double heading = atan2(target.y - pos.y, target.x - pos.x);
        double linearVel = math::clamp(dist * 0.3, -8.0, 8.0);
        double headingError = math::normalizeAngle(heading - r->heading());
        double angularVel = math::clamp(headingError * 2.0, -4.0, 4.0);

        auto ws = math::unicycleToWheelSpeeds(linearVel, angularVel, 2.0, 5.0);
        sendMotorCommand(id, ws.left, ws.right);
        r->setPosition(pos + Vec2(linearVel * 0.05 * cos(heading),
                                   linearVel * 0.05 * sin(heading)), heading);
    }
    for (const auto &id : completedPaths) {
        m_activePaths.remove(id);
        m_pathProgress.remove(id);
    }
}

MultiRobotWorldState MultiRobotCoordinator::buildWorldState(double dt) const {
    MultiRobotWorldState state;
    state.timestamp = QDateTime::currentMSecsSinceEpoch();
    state.deltaTime = dt;
    state.parameters = m_strategyParameters;
    state.neighbors = m_neighborTopology;

    for (const auto &id : m_strategyRobotIds) {
        auto *robot = m_robotManager->robot(id);
        if (!robot || robot->state() != RobotConnectionState::Connected) continue;
        MultiRobotAgentState agent;
        agent.id = id;
        agent.position = robot->position();
        agent.heading = robot->heading();
        agent.sensors = robot->latestSensorData();
        state.agents.append(agent);
    }
    return state;
}

void MultiRobotCoordinator::sendMotorCommand(const RobotId &id, double leftSpeed, double rightSpeed) {
    if (auto *r = m_robotManager->robot(id);
        r && r->state() == RobotConnectionState::Connected) {
        r->setMotorSpeeds(leftSpeed, rightSpeed);
        emit robotCommandIssued(id, leftSpeed, rightSpeed);
    }
}
