#include "PathPlanner.h"
#include <queue>
#include <unordered_map>
#include <cmath>
#include <algorithm>

struct PathPlanner::Node {
    int x, y;
    double g = 1e18, h = 0, f = 1e18;
    Node *parent = nullptr;
    double cost() const { return f; }
};

PathPlanner::PathPlanner(QObject *parent) : QObject(parent) {}

void PathPlanner::setGridSize(int w, int h, double r) {
    m_gridWidth = std::max(1, w); m_gridHeight = std::max(1, h); m_resolution = std::max(0.01, r);
    m_occupancyGrid.resize(h);
    for (auto &row : m_occupancyGrid) { row.resize(m_gridWidth); row.fill(false); }
}

void PathPlanner::setObstacles(const QVector<Vec2> &obstacles, double radius) {
    // Inflate obstacles
    clearObstacles();
    int fillR = std::max(0, static_cast<int>(std::ceil(radius / m_resolution)));
    for (const auto &obs : obstacles) {
        int cx = static_cast<int>(obs.x / m_resolution);
        int cy = static_cast<int>(obs.y / m_resolution);
        for (int dy = -fillR; dy <= fillR; ++dy) {
            for (int dx = -fillR; dx <= fillR; ++dx) {
                int nx = cx + dx, ny = cy + dy;
                if (nx >= 0 && nx < m_gridWidth && ny >= 0 && ny < m_gridHeight) {
                    m_occupancyGrid[ny][nx] = true;
                }
            }
        }
    }
}

void PathPlanner::clearObstacles() {
    for (auto &row : m_occupancyGrid)
        std::fill(row.begin(), row.end(), false);
}

QVector<Vec2> PathPlanner::findPath(const Vec2 &start, const Vec2 &goal) {
    if (m_occupancyGrid.empty()) setGridSize(m_gridWidth, m_gridHeight, m_resolution);
    if (m_resolution <= 0 || !std::isfinite(start.x) || !std::isfinite(start.y)
        || !std::isfinite(goal.x) || !std::isfinite(goal.y)) return {};
    const int sx = static_cast<int>(std::floor(start.x/m_resolution));
    const int sy = static_cast<int>(std::floor(start.y/m_resolution));
    const int gx = static_cast<int>(std::floor(goal.x/m_resolution));
    const int gy = static_cast<int>(std::floor(goal.y/m_resolution));
    auto blocked = [this](int x, int y) {
        return x < 0 || y < 0 || x >= m_gridWidth || y >= m_gridHeight || m_occupancyGrid[y][x];
    };
    if (blocked(sx, sy) || blocked(gx, gy)) return {};
    const int count = m_gridWidth*m_gridHeight;
    std::vector<double> costs(count, 1e18);
    std::vector<int> parents(count, -1);
    std::vector<bool> closed(count, false);
    struct Entry { int id; double g, f; };
    auto compare = [](const Entry &a, const Entry &b) { return a.f > b.f; };
    std::priority_queue<Entry, std::vector<Entry>, decltype(compare)> open(compare);
    auto heuristic = [gx, gy](int x, int y) { return std::hypot(x-gx, y-gy); };
    const int startId = sy*m_gridWidth+sx, goalId = gy*m_gridWidth+gx;
    costs[startId] = 0;
    open.push({startId, 0, heuristic(sx, sy)});
    const int dx[] = {1,1,0,-1,-1,-1,0,1}, dy[] = {0,1,1,1,0,-1,-1,-1};
    while (!open.empty()) {
        const Entry current = open.top(); open.pop();
        if (closed[current.id] || current.g != costs[current.id]) continue;
        if (current.id == goalId) {
            QVector<Vec2> path;
            for (int id = goalId; id >= 0; id = parents[id])
                path.append({(id%m_gridWidth)*m_resolution, (id/m_gridWidth)*m_resolution});
            std::reverse(path.begin(), path.end());
            return path;
        }
        closed[current.id] = true;
        const int x = current.id%m_gridWidth, y = current.id/m_gridWidth;
        for (int i = 0; i < 8; ++i) {
            const int nx = x+dx[i], ny = y+dy[i];
            if (blocked(nx, ny)) continue;
            if (dx[i] && dy[i] && (blocked(x+dx[i], y) || blocked(x, y+dy[i]))) continue;
            const int next = ny*m_gridWidth+nx;
            const double candidate = current.g + (dx[i] && dy[i] ? std::sqrt(2.0) : 1.0);
            if (closed[next] || candidate >= costs[next]) continue;
            costs[next] = candidate;
            parents[next] = current.id;
            open.push({next, candidate, candidate+heuristic(nx, ny)});
        }
    }
    return {};
}

QVector<Vec2> PathPlanner::smoothPath(const QVector<Vec2> &path, double factor) {
    if (path.size() <= 2) return path;

    QVector<Vec2> smoothed = path;
    for (int iter = 0; iter < 5; ++iter) {
        QVector<Vec2> temp = smoothed;
        for (int i = 1; i < smoothed.size() - 1; ++i) {
            temp[i] = smoothed[i] + ((smoothed[i-1] + smoothed[i+1]) * 0.5 - smoothed[i]) * factor;
        }
        smoothed = temp;
    }
    return smoothed;
}
