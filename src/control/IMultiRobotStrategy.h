#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <QVariantMap>

#include "core/SensorData.h"
#include "core/Types.h"

struct MultiRobotAgentState {
    RobotId id;
    Vec2 position;
    double heading = 0.0;
    SensorData sensors;
};

struct MultiRobotWorldState {
    TimestampMs timestamp = 0;
    double deltaTime = 0.05;
    QList<MultiRobotAgentState> agents;
    QMap<RobotId, QList<RobotId>> neighbors;
    QVariantMap parameters;
};

struct MultiRobotStrategyOutput {
    QMap<RobotId, WheelSpeeds> wheelSpeeds;
    QMap<RobotId, Vec2> targetPositions;
    QVariantMap diagnostics;
};

class IMultiRobotStrategy {
public:
    virtual ~IMultiRobotStrategy() = default;
    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual QString description() const = 0;
    virtual void configure(const QVariantMap &parameters) = 0;
    virtual void reset(const MultiRobotWorldState &initialState) = 0;
    virtual MultiRobotStrategyOutput step(const MultiRobotWorldState &state) = 0;
    virtual void stop() = 0;
};

#define IMultiRobotStrategy_iid "org.epuck-mini.control.IMultiRobotStrategy/1.0"
Q_DECLARE_INTERFACE(IMultiRobotStrategy, IMultiRobotStrategy_iid)
