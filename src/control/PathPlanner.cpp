#include "PathPlanner.h"
#include <queue>
#include <unordered_map>
#include <cmath>

struct PathPlanner::Node {
    int x, y;
    double g = 1e18, h = 0, f = 1e18;
    Node *parent = nullptr;
    double cost() const { return f; }
};

PathPlanner::PathPlanner(QObject *parent) : QObject(parent) {}

void PathPlanner::setGridSize(int w, int h, double r) {
    m_gridWidth = w; m_gridHeight = h; m_resolution = r;
    m_occupancyGrid.resize(h);
    for (auto &row : m_occupancyGrid) row.resize(w);
}

void PathPlanner::setObstacles(const QVector<Vec2> &obstacles, double radius) {
    // Inflate obstacles
    int fillR = std::max(1, static_cast<int>(radius / m_resolution));
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
    int sx = static_cast<int>(start.x / m_resolution);
    int sy = static_cast<int>(start.y / m_resolution);
    int gx = static_cast<int>(goal.x / m_resolution);
    int gy = static_cast<int>(goal.y / m_resolution);

    if (sx < 0 || sx >= m_gridWidth || sy < 0 || sy >= m_gridHeight) return {};
    if (gx < 0 || gx >= m_gridWidth || gy < 0 || gy >= m_gridHeight) return {};
    if (!m_occupancyGrid.empty() && (m_occupancyGrid[sy][sx] || m_occupancyGrid[gy][gx])) return {};

    // 8-connected grid
    const int dx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    const int dy[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    const double cost[8] = {1.0, 1.414, 1.0, 1.414, 1.0, 1.414, 1.0, 1.414};

    using NodeId = int; // y * width + x
    std::unordered_map<NodeId, Node> nodes;
    auto id = [this](int x, int y) { return y * m_gridWidth + x; };

    // Comparator for priority queue (min-heap by f-cost)
    auto cmp = [](Node *a, Node *b) { return a->f > b->f; };
    std::priority_queue<Node *, std::vector<Node *>, decltype(cmp)> open(cmp);

    auto heuristic = [](int x1, int y1, int x2, int y2) -> double {
        return std::sqrt(static_cast<double>((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2)));
    };

    Node &startNode = nodes[id(sx, sy)];
    startNode.x = sx; startNode.y = sy; startNode.g = 0;
    startNode.h = heuristic(sx, sy, gx, gy);
    startNode.f = startNode.h;
    open.push(&startNode);

    QVector<Vec2> path;
    NodeId goalId = id(gx, gy);

    while (!open.empty()) {
        Node *current = open.top(); open.pop();

        if (current->x == gx && current->y == gy) {
            // Reconstruct path
            while (current) {
                path.prepend(Vec2(current->x * m_resolution, current->y * m_resolution));
                current = current->parent;
            }
            return path;
        }

        for (int i = 0; i < 8; ++i) {
            int nx = current->x + dx[i], ny = current->y + dy[i];
            if (nx < 0 || nx >= m_gridWidth || ny < 0 || ny >= m_gridHeight) continue;
            if (!m_occupancyGrid.empty() && m_occupancyGrid[ny][nx]) continue;

            double newG = current->g + cost[i];
            NodeId nid = id(nx, ny);
            auto it = nodes.find(nid);
            if (it == nodes.end()) {
                Node &neighbor = nodes[nid];
                neighbor.x = nx; neighbor.y = ny;
                neighbor.parent = current;
                neighbor.g = newG;
                neighbor.h = heuristic(nx, ny, gx, gy);
                neighbor.f = neighbor.g + neighbor.h;
                open.push(&neighbor);
            } else if (newG < it->second.g) {
                it->second.g = newG;
                it->second.parent = current;
                it->second.f = newG + it->second.h;
            }
        }
    }

    return path; // No path found
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
