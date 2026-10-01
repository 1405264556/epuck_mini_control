#include "CoordinationCanvas.h"
#include "core/RobotManager.h"
#include "core/RobotInstance.h"
#include <QContextMenuEvent>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
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
    connect(m_robotManager, &RobotManager::robotConnectionStateChanged, this,
            [this](const RobotId &id, RobotConnectionState state) {
        if (auto *item = m_robotItems.value(id)) item->setOpacity(state == RobotConnectionState::Connected ? 1.0 : 0.4);
    });

    for (auto *robot : m_robotManager->allRobots()) {
        addRobotItem(robot->id());
    }
}

void CoordinationCanvas::drawBackground(QPainter *painter, const QRectF &rect) {
    QGraphicsView::drawBackground(painter, rect);
    painter->fillRect(rect, QColor("#fbfcfc"));

    QPen gridPen(QColor("#e5ebed"), 0.5);
    painter->setPen(gridPen);

    double left = rect.left() - std::fmod(rect.left(), GRID_SPACING);
    double top = rect.top() - std::fmod(rect.top(), GRID_SPACING);
    for (double x = left; x < rect.right(); x += GRID_SPACING)
        painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    for (double y = top; y < rect.bottom(); y += GRID_SPACING)
        painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));

    QPen axisPen(QColor("#b3c1c8"), 1.0);
    painter->setPen(axisPen);
    painter->drawLine(QPointF(0, rect.top()), QPointF(0, rect.bottom()));
    painter->drawLine(QPointF(rect.left(), 0), QPointF(rect.right(), 0));

    painter->save();
    painter->resetTransform();
    QFont labelFont = painter->font();
    labelFont.setPixelSize(11);
    painter->setFont(labelFont);
    painter->setPen(QColor("#738994"));
    const double scale = qMax(0.01, std::abs(transform().m11()));
    const double labelSpacing = GRID_SPACING * qMax(2.0, std::ceil(60.0 / (GRID_SPACING * scale)));
    const double labelLeft = rect.left() - std::fmod(rect.left(), labelSpacing);
    for (double x = labelLeft; x < rect.right(); x += labelSpacing) {
        if (std::abs(x) < 0.1) continue;
        const QPoint point = mapFromScene(x, 0);
        if (point.x() < viewport()->width()-45)
            painter->drawText(point + QPoint(2, -4), QString::number(x, 'f', 0));
    }
    const double labelTop = rect.top() - std::fmod(rect.top(), labelSpacing);
    for (double y = labelTop; y < rect.bottom(); y += labelSpacing) {
        if (std::abs(y) < 0.1) continue;
        const QPoint point = mapFromScene(0, y);
        if (point.y() > 28)
            painter->drawText(point + QPoint(4, -2), QString::number(-y, 'f', 0));
    }
    const QPoint origin = mapFromScene(0, 0);
    painter->setPen(QColor("#566e78"));
    painter->drawText(origin + QPoint(5, -5), "0");
    painter->drawText(QPoint(viewport()->width()-30, origin.y()-5), "+X");
    painter->drawText(QPoint(origin.x()+5, 18), "+Y");
    painter->restore();
}

void CoordinationCanvas::addRobotItem(const RobotId &id) {
    if (m_robotItems.contains(id)) return;

    auto *robot = m_robotManager->robot(id);
    if (!robot) return;

    const QStringList palette{"#087f72", "#b65375", "#3e7aab", "#b28723", "#8b61a4", "#537d42"};
    QColor color(palette[qHash(id) % palette.size()]);

    auto *body = m_scene->addEllipse(-ROBOT_RADIUS, -ROBOT_RADIUS,
        ROBOT_RADIUS * 2, ROBOT_RADIUS * 2,
        QPen(color, 1.5), QBrush(color.lighter(160)));
    body->setZValue(10);
    body->setData(0, id);
    body->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
    body->setCursor(Qt::PointingHandCursor);
    m_robotItems[id] = body;

    auto *heading = m_scene->addLine(0, 0, ROBOT_RADIUS * 1.5, 0,
        QPen(color.darker(140), 1.5));
    heading->setParentItem(body);
    heading->setZValue(11);
    m_headingItems[id] = heading;
    auto *label = new QGraphicsSimpleTextItem(robot->deviceInfo().portName, body);
    label->setBrush(QColor("#405660"));
    label->setFlag(QGraphicsItem::ItemIgnoresTransformations);
    label->setPos(ROBOT_RADIUS + 3, -ROBOT_RADIUS - 8);
    body->setToolTip(robot->name());
    body->setOpacity(robot->state() == RobotConnectionState::Connected ? 1.0 : 0.4);

    updateRobotPose(id, robot->position(), robot->heading());
}

void CoordinationCanvas::removeRobotItem(const RobotId &id) {
    m_headingItems.remove(id);
    if (auto *item = m_robotItems.take(id)) {
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
    const QPointF worldPoint(pos.x, -pos.y);
    item->setPos(worldPoint);
    if (auto *headItem = m_headingItems.value(id)) {
        headItem->setRotation(-heading * 180.0 / M_PI);
    }

    auto &points = m_trailPoints[id];
    if (points.isEmpty() || QLineF(points.last(), worldPoint).length() >= 0.3) points.append(worldPoint);
    else return;
    if (points.size() > 1200) points.remove(0, points.size()-1200);
    QPainterPath trail;
    trail.moveTo(points.first());
    for (int i = 1; i < points.size(); ++i) trail.lineTo(points[i]);
    m_trails[id] = trail;

    auto *trailItem = m_trailItems.value(id, nullptr);
    if (!trailItem) {
        trailItem = m_scene->addPath(trail, QPen(item->pen().color(), 1.0, Qt::SolidLine));
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
            p.x - ROBOT_RADIUS, -p.y - ROBOT_RADIUS,
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
    painterPath.moveTo(waypoints.first().x, -waypoints.first().y);
    for (int i = 1; i < waypoints.size(); ++i) {
        painterPath.lineTo(waypoints[i].x, -waypoints[i].y);
    }

    auto *pathItem = m_scene->addPath(painterPath,
        QPen(QColor("#2185b3"), 1.4, Qt::DashLine));
    pathItem->setZValue(1);
    m_pathItems[id] = pathItem;

    const int stride = qMax(1, static_cast<int>(waypoints.size()/80));
    for (int i = 0; i < waypoints.size(); i += stride) {
        const auto &wp = waypoints[i];
        auto *dot = m_scene->addEllipse(wp.x - 1.2, -wp.y - 1.2, 2.4, 2.4,
            Qt::NoPen, QBrush(QColor("#2185b3")));
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
    m_trailPoints.remove(id);
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
    setGoalPickMode(false);
    setObstacleEditMode(false);
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
    m_scene->setSceneRect(xMin, -yMax, xMax - xMin, yMax - yMin);
}

void CoordinationCanvas::fitAllRobots() {
    QRectF bounds;
    for (auto *item : m_robotItems) {
        bounds = bounds.united(item->sceneBoundingRect());
    }
    for (auto *item : m_pathItems) bounds = bounds.united(item->sceneBoundingRect());
    for (auto *item : m_obstacleItems) bounds = bounds.united(item->sceneBoundingRect());
    if (!bounds.isEmpty()) fitInView(bounds.adjusted(-40, -40, 40, 40), Qt::KeepAspectRatio);
    else fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
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
    if (transform().m11()*factor < 0.35 || transform().m11()*factor > 6.0) return;
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
        emit goalPointSelected(Vec2(qBound(-200.0, scenePos.x(), 200.0), qBound(-200.0, -scenePos.y(), 200.0)));
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
        m_virtualObstacles.append(Vec2(scenePos.x(), -scenePos.y()));
        emit virtualObstaclesChanged(m_virtualObstacles);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        QGraphicsItem *item = itemAt(event->pos());
        while (item && item->parentItem()) item = item->parentItem();
        if (item && item->zValue() == 10) {
            emit robotClicked(item->data(0).toString());
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
            r->setPosition(Vec2(scenePos.x(), -scenePos.y()), r->heading());
        }
    }
    QGraphicsView::mouseMoveEvent(event);
}

void CoordinationCanvas::mouseReleaseEvent(QMouseEvent *event) {
    if (m_dragging) {
        QPointF scenePos = mapToScene(event->pos());
        emit robotMoved(m_dragRobotId, Vec2(scenePos.x(), -scenePos.y()));
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
