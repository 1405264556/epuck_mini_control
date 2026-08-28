#pragma once

#include "core/Types.h"

class CollisionAvoidance {
public:
    static Vec2 computeORCAVelocity(const Vec2 &posA, const Vec2 &velA, double radiusA,
                                     const Vec2 &posB, const Vec2 &velB, double radiusB,
                                     double timeHorizon = 2.0);

    static bool isCollisionImminent(const Vec2 &posA, const Vec2 &velA,
                                     const Vec2 &posB, const Vec2 &velB,
                                     double combinedRadius, double timeHorizon = 2.0);
};
