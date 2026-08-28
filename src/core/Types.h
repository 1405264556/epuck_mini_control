#pragma once

#include <QString>
#include <cstdint>
#include <array>

// ---- Forward declarations ----
using RobotId = QString;
using TimestampMs = qint64;

// ---- Enums ----
enum class RobotConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
};

enum class SensorType {
    Proximity,
    Accelerometer,
    Gyroscope,
    Microphone,
    ToF,
    Camera,
    Battery
};

enum class FormationShape {
    None,
    Line,
    Circle,
    VShape,
    Wedge,
    Column,
    Diamond,
    Custom
};

// ---- 2D Vector ----
struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    Vec2() = default;
    Vec2(double x_, double y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2 &o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2 &o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(double s) const { return {x * s, y * s}; }
    Vec2 operator/(double s) const { return {x / s, y / s}; }
    Vec2 &operator+=(const Vec2 &o) { x += o.x; y += o.y; return *this; }
    Vec2 &operator-=(const Vec2 &o) { x -= o.x; y -= o.y; return *this; }

    double length() const;
    double lengthSquared() const { return x * x + y * y; }
    Vec2 normalized() const;
    double dot(const Vec2 &o) const { return x * o.x + y * o.y; }
    double cross(const Vec2 &o) const { return x * o.y - y * o.x; }
    double distanceTo(const Vec2 &o) const { return (*this - o).length(); }
};

// ---- Wheel speeds (differential drive) ----
struct WheelSpeeds {
    double left = 0.0;   // -1.0 to 1.0
    double right = 0.0;  // -1.0 to 1.0
};
