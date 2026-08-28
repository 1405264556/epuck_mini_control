#pragma once

#include <QObject>
#include <QMap>
#include "IController.h"
#include "core/Types.h"
#include "util/MathUtils.h"

class RobotInstance;

class FormationController : public QObject, public IController {
    Q_OBJECT
public:
    explicit FormationController(QObject *parent = nullptr);

    QString name() const override { return "Formation"; }
    void start(const QList<RobotInstance *> &robots) override;
    void stop() override;
    void update(double dt) override;
    bool isRunning() const override;

    void setShape(FormationShape shape);
    void setSpacing(double cm);
    void setLeader(const RobotId &id);
    void setTargetHeading(double degrees);
    void setTargetPosition(const Vec2 &position);
    void setTargetVelocity(const Vec2 &velocity);

    QMap<RobotId, Vec2> desiredPositions() const;

signals:
    void desiredPositionsChanged(const QMap<RobotId, Vec2> &positions);

private:
    QVector<Vec2> computeFormationOffsets(FormationShape shape, int count, double spacing);

    FormationShape m_shape = FormationShape::Line;
    double m_spacing = 15.0;
    double m_targetHeading = 0.0;
    Vec2 m_targetPosition;
    Vec2 m_targetVelocity;
    RobotId m_leaderId;
    bool m_running = false;
    QList<RobotInstance *> m_robots;
    QMap<RobotId, Vec2> m_desiredPositions;
};
