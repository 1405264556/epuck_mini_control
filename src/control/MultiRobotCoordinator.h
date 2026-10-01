#pragma once

#include <QObject>
#include <QTimer>
#include <QMap>
#include <QStringList>
#include <QVariantMap>
#include "core/Types.h"
#include "IMultiRobotStrategy.h"

class RobotManager;
class RobotInstance;
class FormationController;
class FlockingController;
class PathPlanner;
class MultiRobotStrategyRegistry;

class MultiRobotCoordinator : public QObject {
    Q_OBJECT
public:
    explicit MultiRobotCoordinator(RobotManager *robotManager, QObject *parent = nullptr);

    void startFormation(FormationShape shape, const QList<RobotId> &robotIds,
                        double spacing, const Vec2 &targetPos);
    void startFlocking(const QList<RobotId> &robotIds, const Vec2 &goal);
    void startPathFollowing(const QMap<RobotId, QVector<Vec2>> &robotPaths);
    bool startStrategy(const QString &strategyId, const QList<RobotId> &robotIds,
                       const QVariantMap &parameters = {});
    int reloadStrategyPlugins(const QString &directoryPath = {});
    QStringList availableStrategyIds() const;
    QString strategyDisplayName(const QString &strategyId) const;
    void setNeighborTopology(const QMap<RobotId, QList<RobotId>> &topology);
    void pause();
    void resume();
    void stopAll();
    void emergencyStop();
    void manualDrive(const QMap<RobotId, WheelSpeeds> &commands);
    bool isActive() const;
    bool isPaused() const { return m_paused; }

signals:
    void coordinationStarted();
    void coordinationPaused();
    void coordinationResumed();
    void coordinationStopped();
    void robotCommandIssued(const RobotId &id, double leftSpeed, double rightSpeed);
    void strategyPluginsChanged();
    void strategyError(const QString &message);

private slots:
    void controlLoop();

private:
    void sendMotorCommand(const RobotId &id, double leftSpeed, double rightSpeed);
    void flushMotorCommands();
    MultiRobotWorldState buildWorldState(double dt) const;

    RobotManager *m_robotManager;
    FormationController *m_formationController;
    FlockingController *m_flockingController;
    PathPlanner *m_pathPlanner;
    MultiRobotStrategyRegistry *m_strategyRegistry;
    IMultiRobotStrategy *m_activeStrategy = nullptr;
    QTimer *m_controlTimer;
    bool m_active = false;
    bool m_paused = false;

    QMap<RobotId, QVector<Vec2>> m_activePaths;
    QMap<RobotId, int> m_pathProgress;
    QList<RobotId> m_strategyRobotIds;
    QMap<RobotId, QList<RobotId>> m_neighborTopology;
    QVariantMap m_strategyParameters;
    QList<RobotId> m_participants;
    QMap<RobotId, WheelSpeeds> m_pendingCommands;
};
