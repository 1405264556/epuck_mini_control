#include "CoordinationCanvas.h"
#include "core/RobotManager.h"
#include "core/RobotInstance.h"
#include <QContextMenuEvent>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QLineF>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QtMath>

CoordinationCanvas::CoordinationCanvas(RobotManager *robotMgr, QWidget *parent)
    : QGraphicsView(parent)
    , m_robotManager(robotMgr)
{
    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(-DEFAULT_RANGE, -DEFAULT_RANGE, DEFAULT_RANGE * 2, DEFAULT_RANGE * 2);
    setScene(m_scene);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::NoDrag);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);

    connect(m_robotManager, &RobotManager::robotAdded, this, &CoordinationCanvas::onRobotAdded);
    connect(m_robotManager, &RobotManager::robotRemoved, this, &CoordinationCanvas::onRobotRemoved);
    connect(m_robotManager, &RobotManager::robotSensorDataUpdated,
            this, &CoordinationCanvas::onRobotSensorDataUpdated);
    connect(m_robotManager, &RobotManager::robotPoseChanged,
            this, &CoordinationCanvas::updateRobotPose);

    for (auto *robot : m_robotManager->allRobots()) {
        addRobotItem(robot->id());
    }
}

void CoordinationCanvas::drawBackground(QPainter *painter, const QRectF &rect) {
    QGraphicsView::drawBackground(painter, rect);
    painter->fillRect(rect, QColor("#1e1e2e"));

    QPen gridPen(QColor("#313244"), 0.5);
    painter->setPen(gridPen);

    double left = rect.left() - std::fmod(rect.left(), GRID_SPACING);
    double top = rect.top() - std::fmod(rect.top(), GRID_SPACING);
    for (double x = left; x < rect.right(); x += GRID_SPACING)
        painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    for (double y = top; y < rect.bottom(); y += GRID_SPACING)
        painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));

    QPen axisPen(QColor("#585b70"), 1.5);
    painter->setPen(axisPen);
    painter->drawLine(QPointF(0, rect.top()), QPointF(0, rect.bottom()));
    painter->drawLine(QPointF(rect.left(), 0), QPointF(rect.right(), 0));

    painter->setPen(QColor("#a6adc8"));
    painter->drawText(QPointF(5, -5), "0");
    painter->drawText(QPointF(rect.right() - 32, -5), "+X");
    painter->drawText(QPointF(5, rect.top() + 18), "-Y");

    QFont labelFont = painter->font();
    labelFont.setPointSizeF(7.5);
    painter->setFont(labelFont);
    painter->setPen(QColor("#7f849c"));
    const double labelSpacing = GRID_SPACING * 2.0;
    const double labelLeft = rect.left() - std::fmod(rect.left(), labelSpacing);
    for (double x = labelLeft; x < rect.right(); x += labelSpacing) {
        if (std::abs(x) < 0.1) continue;
        painter->drawText(QPointF(x + 2, -4), QString::number(x, 'f', 0));
    }
    const double labelTop = rect.top() - std::fmod(rect.top(), labelSpacing);
    for (double y = labelTop; y < rect.bottom(); y += labelSpacing) {
        if (std::abs(y) < 0.1) continue;
        painter->drawText(QPointF(4, y - 2), QString::number(y, 'f', 0));
    }
}

void CoordinationCanvas::addRobotItem(const RobotId &id) {
    if (m_robotItems.contains(id)) return;

    auto *robot = m_robotManager->robot(id);
    if (!robot) return;

    QColor color = (robot->state() == RobotConnectionState::Connected)
        ? QColor("#89b4fa") : QColor("#585b70");

    auto *body = m_scene->addEllipse(-ROBOT_RADIUS, -ROBOT_RADIUS,
        ROBOT_RADIUS * 2, ROBOT_RADIUS * 2,
        QPen(color, 2), QBrush(color.darker(200)));
    body->setZValue(10);
    body->setData(0, id);
    body->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
    body->setCursor(Qt::PointingHandCursor);
    m_robotItems[id] = body;

    auto *heading = m_scene->addLine(0, 0, ROBOT_RADIUS * 1.5, 0,
        QPen(QColor("#f9e2af"), 2));
    heading->setParentItem(body);
    heading->setZValue(11);
    m_headingItems[id] = heading;

    updateRobotPose(id, robot->position(), robot->heading());
}

void CoordinationCanvas::removeRobotItem(const RobotId &id) {
    if (auto *item = m_robotItems.take(id)) {
        m_scene->removeItem(item);
        delete item;
    }
    if (auto *item = m_headingItems.take(id)) {
        m_scene->removeItem(item);
        delete item;
    }
    clearPath(id);
    clearTrail(id);
}

void CoordinationCanvas::updateRobotPose(const RobotId &id, const Vec2 &pos, double heading) {
    auto *item = m_robotItems.value(id, nullptr);
    if (!item) return;

    const QPointF oldPos = item->pos();
    item->setPos(pos.x, pos.y);
    if (auto *headItem = m_headingItems.value(id)) {
        headItem->setRotation(heading * 180.0 / M_PI);
    }

    QPainterPath trail = m_trails.value(id);
    if (trail.isEmpty()) {
        trail.moveTo(pos.x, pos.y);
    } else if (QLineF(oldPos, QPointF(pos.x, pos.y)).length() >= 0.8) {
        trail.lineTo(pos.x, pos.y);
    }
    m_trails[id] = trail;

    auto *trailItem = m_trailItems.value(id, nullptr);
    if (!trailItem) {
        trailItem = m_scene->addPath(trail, QPen(QColor("#fab387"), 1.2, Qt::SolidLine));
        trailItem->setZValue(0.5);
        m_trailItems[id] = trailItem;
    } else {
        trailItem->setPath(trail);
    }
}

void CoordinationCanvas::showFormationPreview(FormationShape, const QVector<Vec2> &positions) {
    hideFormationPreview();
    for (const auto &p : positions) {
        auto *item = m_scene->addEllipse(
            p.x - ROBOT_RADIUS, p.y - ROBOT_RADIUS,
            ROBOT_RADIUS * 2, ROBOT_RADIUS * 2,
            QPen(QColor("#cba6f7"), 1.5, Qt::DashLine), Qt::NoBrush);
        item->setZValue(5);
        m_formationPreviewItems.append(item);
    }
}

void CoordinationCanvas::hideFormationPreview() {
    for (auto *item : m_formationPreviewItems) {
        m_scene->removeItem(item);
        delete item;
    }
    m_formationPreviewItems.clear();
}

void CoordinationCanvas::showPath(const RobotId &id, const QVector<Vec2> &waypoints) {
    clearPath(id);
    if (waypoints.size() < 2) return;

    QPainterPath painterPath;
    painterPath.moveTo(waypoints.first().x, waypoints.first().y);
    for (int i = 1; i < waypoints.size(); ++i) {
        painterPath.lineTo(waypoints[i].x, waypoints[i].y);
    }

    auto *pathItem = m_scene->addPath(painterPath,
        QPen(QColor("#a6e3a1"), 1.8, Qt::SolidLine));
    pathItem->setZValue(1);
    m_pathItems[id] = pathItem;

    for (const auto &wp : waypoints) {
        auto *dot = m_scene->addEllipse(wp.x - 1.8, wp.y - 1.8, 3.6, 3.6,
            Qt::NoPen, QBrush(QColor("#a6e3a1")));
        dot->setZValue(2);
        dot->setParentItem(pathItem);
    }
}

void CoordinationCanvas::clearPath(const RobotId &id) {
    if (auto *item = m_pathItems.take(id)) {
        m_scene->removeItem(item);
        delete item;
    }
}

void CoordinationCanvas::clearTrail(const RobotId &id) {
    if (auto *item = m_trailItems.take(id)) {
        m_scene->removeItem(item);
        delete item;
    }
    m_trails.remove(id);
}

void CoordinationCanvas::setGoalPickMode(bool enabled) {
    m_goalPickMode = enabled;
    if (enabled) m_obstacleEditMode = false;
    viewport()->setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
}

void CoordinationCanvas::setObstacleEditMode(bool enabled) {
    m_obstacleEditMode = enabled;
    if (enabled) m_goalPickMode = false;
    viewport()->setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
}

void CoordinationCanvas::clearCanvasOverlays() {
    for (const auto &id : m_pathItems.keys()) clearPath(id);
    for (const auto &id : m_trailItems.keys()) clearTrail(id);
    hideFormationPreview();
    if (m_goalMarker) {
        m_scene->removeItem(m_goalMarker);
        delete m_goalMarker;
        m_goalMarker = nullptr;
    }
    for (auto *item : m_obstacleItems) {
        m_scene->removeItem(item);
        delete item;
    }
    m_obstacleItems.clear();
    m_virtualObstacles.clear();
    emit virtualObstaclesChanged(m_virtualObstacles);
}

void CoordinationCanvas::setCoordinateRange(double xMin, double xMax, double yMin, double yMax) {
    m_scene->setSceneRect(xMin, yMin, xMax - xMin, yMax - yMin);
}

void CoordinationCanvas::fitAllRobots() {
    QRectF bounds;
    for (auto *item : m_robotItems) {
        bounds = bounds.united(item->sceneBoundingRect());
    }
    if (!bounds.isEmpty()) fitInView(bounds.adjusted(-40, -40, 40, 40), Qt::KeepAspectRatio);
}

void CoordinationCanvas::onRobotSensorDataUpdated(const RobotId &id, const SensorData &) {
    if (auto *r = m_robotManager->robot(id)) {
        updateRobotPose(id, r->position(), r->heading());
    }
}

void CoordinationCanvas::onRobotAdded(const RobotId &id) { addRobotItem(id); }
void CoordinationCanvas::onRobotRemoved(const RobotId &id) { removeRobotItem(id); }

void CoordinationCanvas::wheelEvent(QWheelEvent *event) {
    double factor = (event->angleDelta().y() > 0) ? 1.15 : 0.87;
    scale(factor, factor);
}

void CoordinationCanvas::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::MiddleButton) {
        setDragMode(QGraphicsView::ScrollHandDrag);
        QMouseEvent fake(QEvent::MouseButtonPress, event->pos(), Qt::LeftButton,
                         Qt::LeftButton, event->modifiers());
        QGraphicsView::mousePressEvent(&fake);
        return;
    }

    if (m_goalPickMode && event->button() == Qt::LeftButton) {
        const QPointF scenePos = mapToScene(event->pos());
        if (!m_goalMarker) {
            m_goalMarker = m_scene->addEllipse(-4, -4, 8, 8,
                QPen(QColor("#f38ba8"), 1.8), QBrush(QColor(243, 139, 168, 90)));
            m_goalMarker->setZValue(8);
        }
        m_goalMarker->setPos(scenePos);
        setGoalPickMode(false);
        emit goalPointSelected(Vec2(scenePos.x(), scenePos.y()));
        event->accept();
        return;
    }

    if (m_obstacleEditMode && event->button() == Qt::LeftButton) {
        const QPointF scenePos = mapToScene(event->pos());
        constexpr double size = 12.0;
        auto *obstacle = m_scene->addRect(scenePos.x() - size * 0.5,
                                          scenePos.y() - size * 0.5,
                                          size, size,
                                          QPen(QColor("#f38ba8"), 1.2),
                                          QBrush(QColor(243, 139, 168, 100)));
        obstacle->setZValue(3);
        m_obstacleItems.append(obstacle);
        m_virtualObstacles.append(Vec2(scenePos.x(), scenePos.y()));
        emit virtualObstaclesChanged(m_virtualObstacles);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        QGraphicsItem *item = itemAt(event->pos());
        if (item && item->zValue() == 10) {
            m_dragging = true;
            m_dragRobotId = item->data(0).toString();
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void CoordinationCanvas::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging) {
        QPointF scenePos = mapToScene(event->pos());
        if (auto *r = m_robotManager->robot(m_dragRobotId)) {
            r->setPosition(Vec2(scenePos.x(), scenePos.y()), r->heading());
        }
    }
    QGraphicsView::mouseMoveEvent(event);
}

void CoordinationCanvas::mouseReleaseEvent(QMouseEvent *event) {
    if (m_dragging) {
        QPointF scenePos = mapToScene(event->pos());
        emit robotMoved(m_dragRobotId, Vec2(scenePos.x(), scenePos.y()));
        m_dragging = false;
    }
    if (event->button() == Qt::MiddleButton)
        setDragMode(QGraphicsView::NoDrag);
    QGraphicsView::mouseReleaseEvent(event);
}

void CoordinationCanvas::contextMenuEvent(QContextMenuEvent *event) {
    QGraphicsItem *item = itemAt(event->pos());
    QMenu menu;
    if (item && item->zValue() == 10) {
        RobotId id = item->data(0).toString();
        menu.addAction("设为领航者", [this, id]() {
            emit robotClicked(id);
        });
        menu.addSeparator();
    }
    menu.addAction("选择路径目标点", this, [this]() {
        setGoalPickMode(true);
    });
    menu.addAction("添加虚拟障碍", this, [this]() {
        setObstacleEditMode(true);
    });
    menu.addAction("适应全部机器人", this, &CoordinationCanvas::fitAllRobots);
    menu.addAction("清除规划路径", this, [this]() {
        for (const auto &id : m_pathItems.keys()) clearPath(id);
    });
    menu.addAction("清除行进轨迹", this, [this]() {
        for (const auto &id : m_trailItems.keys()) clearTrail(id);
    });
    menu.addAction("清理画布", this, &CoordinationCanvas::clearCanvasOverlays);
    menu.exec(event->globalPos());
    emit canvasContextMenuRequested(mapToScene(event->pos()));
}
