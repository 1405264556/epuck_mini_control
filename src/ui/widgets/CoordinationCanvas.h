#pragma once
#include <QGraphicsView>
#include <QMap>
#include <QPainterPath>
#include "core/Types.h"

class QGraphicsScene;
class QGraphicsEllipseItem;
class QGraphicsPathItem;
class QGraphicsLineItem;
class QGraphicsRectItem;
class RobotManager;
class SensorData;

class CoordinationCanvas : public QGraphicsView {
    Q_OBJECT
public:
    explicit CoordinationCanvas(RobotManager *robotMgr, QWidget *parent = nullptr);

    void addRobotItem(const RobotId &id);
    void removeRobotItem(const RobotId &id);
    void updateRobotPose(const RobotId &id, const Vec2 &pos, double heading);
    void showFormationPreview(FormationShape shape, const QVector<Vec2> &positions);
    void hideFormationPreview();
    void showPath(const RobotId &id, const QVector<Vec2> &waypoints);
    void clearPath(const RobotId &id);
    void clearTrail(const RobotId &id);
    void setGoalPickMode(bool enabled);
    void setObstacleEditMode(bool enabled);
    void clearCanvasOverlays();
    QVector<Vec2> virtualObstacles() const { return m_virtualObstacles; }
    void setCoordinateRange(double xMin, double xMax, double yMin, double yMax);
    void fitAllRobots();

public slots:
    void onRobotSensorDataUpdated(const RobotId &id, const SensorData &data);
    void onRobotAdded(const RobotId &id);
    void onRobotRemoved(const RobotId &id);

signals:
    void robotClicked(const RobotId &id);
    void robotMoved(const RobotId &id, const Vec2 &newPos);
    void goalPointSelected(const Vec2 &pos);
    void virtualObstaclesChanged(const QVector<Vec2> &obstacles);
    void canvasContextMenuRequested(const QPointF &scenePos);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QGraphicsScene *m_scene;
    QMap<RobotId, QGraphicsEllipseItem *> m_robotItems;
    QMap<RobotId, QGraphicsLineItem *> m_headingItems;
    QVector<QGraphicsEllipseItem *> m_formationPreviewItems;
    QMap<RobotId, QGraphicsPathItem *> m_pathItems;
    QMap<RobotId, QGraphicsPathItem *> m_trailItems;
    QMap<RobotId, QPainterPath> m_trails;
    QGraphicsEllipseItem *m_goalMarker = nullptr;
    QVector<QGraphicsRectItem *> m_obstacleItems;
    QVector<Vec2> m_virtualObstacles;
    static constexpr double GRID_SPACING = 20.0;
    static constexpr double DEFAULT_RANGE = 200.0;
    static constexpr double ROBOT_RADIUS = 5.0;
    RobotManager *m_robotManager;
    bool m_goalPickMode = false;
    bool m_obstacleEditMode = false;
    bool m_dragging = false;
    RobotId m_dragRobotId;
};
