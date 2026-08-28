#pragma once

#include <cmath>
#include "core/Types.h"

namespace math {

constexpr double PI = 3.14159265358979323846;
constexpr double RAD_TO_DEG = 180.0 / PI;
constexpr double DEG_TO_RAD = PI / 180.0;

inline double degToRad(double deg) { return deg * DEG_TO_RAD; }
inline double radToDeg(double rad) { return rad * RAD_TO_DEG; }

double normalizeAngle(double angle);
double angleDifference(double a, double b);
double clamp(double value, double minVal, double maxVal);
double lerp(double a, double b, double t);

Vec2 rotateVec2(const Vec2 &v, double angle);

// Differential drive kinematics (wheel radius in cm, wheelbase in cm)
WheelSpeeds unicycleToWheelSpeeds(double linearVel, double angularVel,
                                   double wheelRadius, double wheelBase);

void wheelSpeedsToUnicycle(double leftSpeed, double rightSpeed,
                            double wheelRadius, double wheelBase,
                            double &linearVel, double &angularVel);

// Simple PID controller
class PID {
public:
    PID(double kp = 1.0, double ki = 0.0, double kd = 0.0, double maxIntegral = 100.0);

    double update(double setpoint, double measurement, double dt);
    void reset();
    void setGains(double kp, double ki, double kd);

    double kp() const { return m_kp; }
    double ki() const { return m_ki; }
    double kd() const { return m_kd; }

private:
    double m_kp, m_ki, m_kd, m_maxIntegral;
    double m_integral = 0.0;
    double m_prevError = 0.0;
};

} // namespace math

// Vec2 inline implementations
inline double Vec2::length() const { return std::sqrt(x * x + y * y); }
inline Vec2 Vec2::normalized() const {
    double len = length();
    return (len > 1e-12) ? Vec2{x / len, y / len} : Vec2{};
}
