#include "MathUtils.h"

namespace math {

double normalizeAngle(double angle) {
    while (angle > PI) angle -= 2.0 * PI;
    while (angle < -PI) angle += 2.0 * PI;
    return angle;
}

double angleDifference(double a, double b) {
    return normalizeAngle(a - b);
}

double clamp(double value, double minVal, double maxVal) {
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return value;
}

double lerp(double a, double b, double t) {
    return a + t * (b - a);
}

Vec2 rotateVec2(const Vec2 &v, double angle) {
    double c = std::cos(angle);
    double s = std::sin(angle);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

WheelSpeeds unicycleToWheelSpeeds(double linearVel, double angularVel,
                                   double wheelRadius, double wheelBase) {
    double halfBase = wheelBase / 2.0;
    double rightVel = (linearVel + angularVel * halfBase) / wheelRadius;
    double leftVel = (linearVel - angularVel * halfBase) / wheelRadius;
    double maxAbs = std::max(std::abs(leftVel), std::abs(rightVel));
    if (maxAbs > 1.0) {
        leftVel /= maxAbs;
        rightVel /= maxAbs;
    }
    return {leftVel, rightVel};
}

void wheelSpeedsToUnicycle(double leftSpeed, double rightSpeed,
                            double /*wheelRadius*/, double wheelBase,
                            double &linearVel, double &angularVel) {
    linearVel = (rightSpeed + leftSpeed) / 2.0;
    angularVel = (rightSpeed - leftSpeed) / wheelBase;
}

// ---- PID ----
PID::PID(double kp, double ki, double kd, double maxIntegral)
    : m_kp(kp), m_ki(ki), m_kd(kd), m_maxIntegral(maxIntegral) {}

double PID::update(double setpoint, double measurement, double dt) {
    if (dt <= 0.0) return 0.0;
    double error = setpoint - measurement;
    m_integral += error * dt;
    m_integral = clamp(m_integral, -m_maxIntegral, m_maxIntegral);
    double derivative = (error - m_prevError) / dt;
    m_prevError = error;
    return m_kp * error + m_ki * m_integral + m_kd * derivative;
}

void PID::reset() {
    m_integral = 0.0;
    m_prevError = 0.0;
}

void PID::setGains(double kp, double ki, double kd) {
    m_kp = kp; m_ki = ki; m_kd = kd;
}

} // namespace math
