#include "FlockingController.h"
#include "core/RobotInstance.h"
#include "util/MathUtils.h"

FlockingController::FlockingController(QObject *parent)
    : QObject(parent)
{
}

void FlockingController::start(const QList<RobotInstance *> &robots) {
    m_robots = robots;
    m_running = true;
}

void FlockingController::stop() { m_running = false; }

bool FlockingController::isRunning() const { return m_running; }

void FlockingController::update(double dt) {
    if (!m_running || m_robots.isEmpty()) return;

    QMap<RobotId, Vec2> newVelocities;
    QMap<RobotId, Vec2> positions;

    for (auto *r : m_robots) {
        positions[r->id()] = r->position();
    }

    for (auto *r : m_robots) {
        Vec2 pos = r->position();
        Vec2 separation(0, 0);
        Vec2 alignment(0, 0);
        Vec2 cohesion(0, 0);
        Vec2 goalSteer(0, 0);
        int neighborCount = 0;

        for (auto *other : m_robots) {
            if (other == r) continue;
            Vec2 diff = pos - other->position();
            double dist = diff.length();

            if (dist < m_neighborRadius && dist > 0.01) {
                // Separation: steer away from nearby neighbors (inverse distance)
                separation = separation + diff.normalized() * (1.0 / std::max(dist, 0.1));
                // Alignment: steer toward average velocity of neighbors
                const auto speeds = other->commandedSpeeds();
                alignment += Vec2(std::cos(other->heading()), std::sin(other->heading()))
                    * ((speeds.left + speeds.right) * 6.44);
                // Cohesion: steer toward center of neighbors
                cohesion = cohesion + other->position();
                neighborCount++;
            }
        }

        if (neighborCount > 0) {
            separation = separation * (1.0 / neighborCount);
            cohesion = cohesion * (1.0 / neighborCount);
            cohesion = (cohesion - pos).normalized();
            alignment = alignment * (1.0 / neighborCount);
        }

        // Goal seeking
        goalSteer = (m_goalPosition - pos).normalized();

        // Weighted combination
        Vec2 desiredVel = separation * m_separationWeight
                        + alignment * m_alignmentWeight
                        + cohesion * m_cohesionWeight
                        + goalSteer * m_goalWeight;

        double speed = desiredVel.length();
        if (speed > m_maxSpeed) {
            desiredVel = desiredVel * (m_maxSpeed / speed);
        }

        newVelocities[r->id()] = desiredVel;
    }

    // Convert to wheel speeds and emit
    QMap<RobotId, WheelSpeeds> wheelSpeeds;
    for (auto *r : m_robots) {
        Vec2 vel = newVelocities.value(r->id());
        // Convert velocity vector to differential drive
        double linearVel = vel.length();
        double angularVel = 0.0;
        if (linearVel > 0.01) {
            Vec2 heading(cos(r->heading()), sin(r->heading()));
            double angleError = atan2(vel.y, vel.x) - atan2(heading.y, heading.x);
            const double error = math::normalizeAngle(angleError);
            angularVel = error * 2.0;
            linearVel *= qMax(0.0, std::cos(error));
        }
        auto ws = math::unicycleToWheelSpeeds(
            math::clamp(linearVel, -m_maxSpeed, m_maxSpeed),
            math::clamp(angularVel, -4.0, 4.0), 12.88, 5.3);
        wheelSpeeds[r->id()] = ws;
    }

    if (!wheelSpeeds.isEmpty()) {
        emit velocitiesComputed(wheelSpeeds);
    }
}
