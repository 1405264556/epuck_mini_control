#include "FormationController.h"
#include "core/RobotInstance.h"
#include "util/MathUtils.h"

FormationController::FormationController(QObject *parent)
    : QObject(parent)
{
}

void FormationController::start(const QList<RobotInstance *> &robots) {
    m_robots = robots;
    m_running = true;
    update(0.05);
}

void FormationController::stop() {
    m_running = false;
}

bool FormationController::isRunning() const { return m_running; }

void FormationController::update(double dt) {
    if (!m_running || m_robots.isEmpty()) return;

    QVector<Vec2> offsets = computeFormationOffsets(m_shape, m_robots.size(), m_spacing);
    if (offsets.isEmpty()) return;

    m_desiredPositions.clear();
    for (int i = 0; i < m_robots.size() && i < offsets.size(); ++i) {
        Vec2 worldPos = m_targetPosition + math::rotateVec2(offsets[i], m_targetHeading);
        m_desiredPositions[m_robots[i]->id()] = worldPos;
    }

    emit desiredPositionsChanged(m_desiredPositions);
}

void FormationController::setShape(FormationShape shape) { m_shape = shape; }
void FormationController::setSpacing(double cm) { m_spacing = cm; }
void FormationController::setLeader(const RobotId &id) { m_leaderId = id; }
void FormationController::setTargetHeading(double degrees) { m_targetHeading = math::degToRad(degrees); }
void FormationController::setTargetPosition(const Vec2 &position) { m_targetPosition = position; }
void FormationController::setTargetVelocity(const Vec2 &velocity) { m_targetVelocity = velocity; }

QMap<RobotId, Vec2> FormationController::desiredPositions() const { return m_desiredPositions; }

QVector<Vec2> FormationController::computeFormationOffsets(FormationShape shape, int count,
                                                            double spacing) {
    QVector<Vec2> offsets;
    if (count <= 0) return offsets;
    offsets.resize(count);

    switch (shape) {
    case FormationShape::Line:
        for (int i = 0; i < count; ++i)
            offsets[i] = Vec2(0, (i - (count - 1) / 2.0) * spacing);
        break;
    case FormationShape::Circle: {
        double radius = spacing * count / (2.0 * math::PI);
        for (int i = 0; i < count; ++i) {
            double angle = 2.0 * math::PI * i / count;
            offsets[i] = Vec2(radius * std::cos(angle), radius * std::sin(angle));
        }
        break;
    }
    case FormationShape::VShape:
        offsets[0] = Vec2(0, 0);
        for (int i = 1; i < count; ++i) {
            const int rank = (i + 1) / 2;
            offsets[i] = Vec2(-rank * spacing, (i % 2 == 1 ? 1 : -1) * rank * spacing);
        }
        break;
    case FormationShape::Wedge:
        for (int i = 0; i < count; ++i) {
            double side = (i % 2 == 0 ? 1.0 : -1.0);
            offsets[i] = Vec2(-i * spacing, side * i * spacing * 0.5);
        }
        break;
    case FormationShape::Column:
        for (int i = 0; i < count; ++i)
            offsets[i] = Vec2(-i * spacing, 0);
        break;
    case FormationShape::Diamond:
        if (count >= 1) offsets[0] = Vec2(0, -spacing);
        if (count >= 2) offsets[1] = Vec2(spacing, 0);
        if (count >= 3) offsets[2] = Vec2(0, spacing);
        if (count >= 4) offsets[3] = Vec2(-spacing, 0);
        for (int i = 4; i < count; ++i)
            offsets[i] = Vec2(i * spacing, 0);
        break;
    default:
        break;
    }
    return offsets;
}
