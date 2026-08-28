#pragma once

#include <QString>
#include <QList>

class RobotInstance;

class IController {
public:
    virtual ~IController() = default;
    virtual QString name() const = 0;
    virtual void start(const QList<RobotInstance *> &robots) = 0;
    virtual void stop() = 0;
    virtual void update(double dt) = 0;
    virtual bool isRunning() const = 0;
};
