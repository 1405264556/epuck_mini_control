#pragma once

#include <QObject>
#include <QMap>
#include "IController.h"
#include "core/Types.h"

class RobotInstance;

class FlockingController : public QObject, public IController {
    Q_OBJECT
public:
    explicit FlockingController(QObject *parent = nullptr);

    QString name() const override { return "Flocking"; }
    void start(const QList<RobotInstance *> &robots) override;
    void stop() override;
    void update(double dt) override;
    bool isRunning() const override;

    void setSeparationWeight(double w) { m_separationWeight = w; }
    void setAlignmentWeight(double w) { m_alignmentWeight = w; }
    void setCohesionWeight(double w) { m_cohesionWeight = w; }
    void setGoalWeight(double w) { m_goalWeight = w; }
    void setGoalPosition(const Vec2 &goal) { m_goalPosition = goal; }
    void setNeighborRadius(double cm) { m_neighborRadius = cm; }
    void setMaxSpeed(double speed) { m_maxSpeed = speed; }

signals:
    void velocitiesComputed(const QMap<RobotId, WheelSpeeds> &wheelSpeeds);

private:
    double m_separationWeight = 1.5;
    double m_alignmentWeight = 1.0;
    double m_cohesionWeight = 1.0;
    double m_goalWeight = 0.5;
    Vec2 m_goalPosition;
    double m_neighborRadius = 30.0;
    double m_maxSpeed = 10.0;
    bool m_running = false;
    QList<RobotInstance *> m_robots;
};
