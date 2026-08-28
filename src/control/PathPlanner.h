#pragma once

#include <QObject>
#include <QVector>
#include "core/Types.h"

class PathPlanner : public QObject {
    Q_OBJECT
public:
    explicit PathPlanner(QObject *parent = nullptr);

    void setGridSize(int width, int height, double resolution = 1.0);
    void setObstacles(const QVector<Vec2> &obstacles, double radius);
    void clearObstacles();
    QVector<Vec2> findPath(const Vec2 &start, const Vec2 &goal);
    QVector<Vec2> smoothPath(const QVector<Vec2> &path, double smoothingFactor = 0.5);
    const QVector<QVector<bool>> &occupancyGrid() const { return m_occupancyGrid; }

signals:
    void pathComputed(const QVector<Vec2> &path);

private:
    struct Node;
    int m_gridWidth = 200, m_gridHeight = 200;
    double m_resolution = 1.0;
    QVector<QVector<bool>> m_occupancyGrid;
};
