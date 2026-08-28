#include "CollisionAvoidance.h"
#include <cmath>

Vec2 CollisionAvoidance::computeORCAVelocity(const Vec2 &posA, const Vec2 &velA, double radiusA,
                                               const Vec2 &posB, const Vec2 &velB, double radiusB,
                                               double timeHorizon) {
    Vec2 relPos = posB - posA;
    Vec2 relVel = velA - velB;

    double distSq = relPos.lengthSquared();
    double combinedRadius = radiusA + radiusB;
    double combinedRadiusSq = combinedRadius * combinedRadius;

    if (distSq > combinedRadiusSq) {
        // No collision currently, check if on collision course
        double t = -relPos.dot(relVel) / std::max(relVel.lengthSquared(), 1e-6);
        if (t <= 0.0 || t > timeHorizon) {
            return velA; // No collision imminent
        }

        Vec2 projPoint = posA + velA * t;
        Vec2 projPointB = posB + velB * t;
        Vec2 distAtClosest = projPointB - projPoint;

        if (distAtClosest.lengthSquared() >= combinedRadiusSq) {
            return velA; // Miss distance sufficient
        }
    }

    // Compute avoidance direction
    Vec2 avoidDir = relPos.normalized();

    // Project preferred velocity onto collision-free half-plane
    double velAlongAvoid = velA.dot(avoidDir);
    if (velAlongAvoid < 0.0) {
        // Already moving away
        return velA;
    }

    // Subtract the component moving toward the obstacle
    Vec2 adjustedVel = velA - avoidDir * velAlongAvoid * 0.8;

    // Add a slight perpendicular component to slide past
    Vec2 perpDir(-avoidDir.y, avoidDir.x);
    adjustedVel = adjustedVel + perpDir * 0.3;

    double speed = adjustedVel.length();
    if (speed > 10.0) {
        adjustedVel = adjustedVel * (10.0 / speed);
    }

    return adjustedVel;
}

bool CollisionAvoidance::isCollisionImminent(const Vec2 &posA, const Vec2 &velA,
                                               const Vec2 &posB, const Vec2 &velB,
                                               double combinedRadius, double timeHorizon) {
    Vec2 relPos = posB - posA;
    Vec2 relVel = velA - velB;

    double distSq = relPos.lengthSquared();
    double combinedRadiusSq = combinedRadius * combinedRadius;

    if (distSq <= combinedRadiusSq) return true;

    double relVelSq = relVel.lengthSquared();
    if (relVelSq < 1e-6) return false;

    double t = -relPos.dot(relVel) / relVelSq;
    if (t < 0.0 || t > timeHorizon) return false;

    Vec2 closestA = posA + velA * t;
    Vec2 closestB = posB + velB * t;
    return (closestB - closestA).lengthSquared() < combinedRadiusSq;
}
