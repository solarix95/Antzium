#include "flowfield.h"

#include "worldmap.h"

#include <QSize>

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

namespace
{
constexpr double Infinity = std::numeric_limits<double>::infinity();

struct QueueNode
{
    int x = 0;
    int y = 0;
    double cost = 0.0;

    bool operator>(const QueueNode& other) const
    {
        return cost > other.cost;
    }
};
}

void FlowField::rebuild(const WorldMap& map, const QPointF& goalWorldPos)
{
    clear();

    if (!map.isValid()) {
        return;
    }

    m_widthTiles = map.widthTiles();
    m_heightTiles = map.heightTiles();
    m_tileSize = map.tileSize();

    m_cells.resize(m_widthTiles * m_heightTiles);

    for (Cell& cell : m_cells) {
        cell.cost = Infinity;
        cell.direction = QPointF(0.0, 0.0);
        cell.reachable = false;
    }

    QPoint goalTile = worldToTile(goalWorldPos);

    if (!isInside(goalTile.x(), goalTile.y()) || !map.isWalkableTile(goalTile.x(), goalTile.y())) {
        return;
    }

    std::priority_queue<
        QueueNode,
        std::vector<QueueNode>,
        std::greater<QueueNode>
        > open;

    const int goalIndex = index(goalTile.x(), goalTile.y());
    m_cells[goalIndex].cost = 0.0;
    m_cells[goalIndex].reachable = true;

    open.push({ goalTile.x(), goalTile.y(), 0.0 });

    const int dx[8] = { 1, -1,  0,  0,  1,  1, -1, -1 };
    const int dy[8] = { 0,  0,  1, -1,  1, -1,  1, -1 };

    while (!open.empty()) {
        const QueueNode current = open.top();
        open.pop();

        const int currentIndex = index(current.x, current.y);

        if (current.cost > m_cells[currentIndex].cost) {
            continue;
        }

        for (int n = 0; n < 8; ++n) {
            const int nx = current.x + dx[n];
            const int ny = current.y + dy[n];

            if (!isInside(nx, ny)) {
                continue;
            }

            if (!map.isWalkableTile(nx, ny)) {
                continue;
            }

            if (!diagonalAllowed(map, current.x, current.y, nx, ny)) {
                continue;
            }

            const double stepCost =
                movementCost(map, current.x, current.y, nx, ny);

            const double newCost = current.cost + stepCost;

            const int ni = index(nx, ny);

            if (newCost < m_cells[ni].cost) {
                m_cells[ni].cost = newCost;
                m_cells[ni].reachable = true;
                open.push({ nx, ny, newCost });
            }
        }
    }

    // Richtung pro Tile berechnen:
    // Jedes Tile zeigt auf den Nachbarn mit niedrigerem Cost.
    for (int y = 0; y < m_heightTiles; ++y) {
        for (int x = 0; x < m_widthTiles; ++x) {
            const int i = index(x, y);

            if (!m_cells[i].reachable) {
                continue;
            }

            if (x == goalTile.x() && y == goalTile.y()) {
                m_cells[i].direction = QPointF(0.0, 0.0);
                continue;
            }

            double bestCost = m_cells[i].cost;
            QPoint bestTile(x, y);

            for (int n = 0; n < 8; ++n) {
                const int nx = x + dx[n];
                const int ny = y + dy[n];

                if (!isInside(nx, ny)) {
                    continue;
                }

                if (!map.isWalkableTile(nx, ny)) {
                    continue;
                }

                if (!diagonalAllowed(map, x, y, nx, ny)) {
                    continue;
                }

                const int ni = index(nx, ny);

                if (!m_cells[ni].reachable) {
                    continue;
                }

                if (m_cells[ni].cost < bestCost) {
                    bestCost = m_cells[ni].cost;
                    bestTile = QPoint(nx, ny);
                }
            }

            const QPointF from = tileCenterWorld(x, y);
            const QPointF to = tileCenterWorld(bestTile.x(), bestTile.y());

            const QPointF delta(to.x() - from.x(), to.y() - from.y());
            const double len = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());

            if (len > 0.0001) {
                m_cells[i].direction = QPointF(delta.x() / len, delta.y() / len);
            }
        }
    }
}

void FlowField::clear()
{
    m_widthTiles = 0;
    m_heightTiles = 0;
    m_tileSize = 10;
    m_cells.clear();
}

bool FlowField::isValid() const
{
    return m_widthTiles > 0 && m_heightTiles > 0 && !m_cells.isEmpty();
}

QPointF FlowField::directionAtWorld(const QPointF& worldPos) const
{
    if (!isValid()) {
        return QPointF(0.0, 0.0);
    }

    const QPoint tile = worldToTile(worldPos);

    if (!isInside(tile.x(), tile.y())) {
        return QPointF(0.0, 0.0);
    }

    const Cell& cell = m_cells[index(tile.x(), tile.y())];

    if (!cell.reachable) {
        return QPointF(0.0, 0.0);
    }

    return cell.direction;
}

double FlowField::costAtWorld(const QPointF& worldPos) const
{
    if (!isValid()) {
        return Infinity;
    }

    const QPoint tile = worldToTile(worldPos);

    if (!isInside(tile.x(), tile.y())) {
        return Infinity;
    }

    const Cell& cell = m_cells[index(tile.x(), tile.y())];

    if (!cell.reachable) {
        return Infinity;
    }

    return cell.cost;
}

bool FlowField::isReachableWorld(const QPointF& worldPos) const
{
    return std::isfinite(costAtWorld(worldPos));
}

bool FlowField::isInside(int tx, int ty) const
{
    return tx >= 0 && ty >= 0 && tx < m_widthTiles && ty < m_heightTiles;
}

int FlowField::index(int tx, int ty) const
{
    return ty * m_widthTiles + tx;
}

QPoint FlowField::worldToTile(const QPointF& worldPos) const
{
    return QPoint(
        static_cast<int>(std::floor(worldPos.x() / m_tileSize)),
        static_cast<int>(std::floor(worldPos.y() / m_tileSize))
        );
}

QPointF FlowField::tileCenterWorld(int tx, int ty) const
{
    return QPointF(
        tx * m_tileSize + m_tileSize * 0.5,
        ty * m_tileSize + m_tileSize * 0.5
        );
}

bool FlowField::diagonalAllowed(const WorldMap& map, int x, int y, int nx, int ny) const
{
    const int dx = nx - x;
    const int dy = ny - y;

    if (std::abs(dx) + std::abs(dy) != 2) {
        return true;
    }

    // Kein diagonales "durch die Ecke schneiden".
    return map.isWalkableTile(x + dx, y)
           && map.isWalkableTile(x, y + dy);
}

double FlowField::movementCost(const WorldMap& map, int fromX, int fromY, int toX, int toY) const
{
    const QPointF toCenter = tileCenterWorld(toX, toY);

    const double terrainSpeed = std::max(0.05, map.speedAtWorld(toCenter));
    const double terrainCost = 1.0 / terrainSpeed;

    const bool diagonal =
        fromX != toX && fromY != toY;

    const double distanceCost = diagonal ? 1.41421356237 : 1.0;

    return terrainCost * distanceCost;
}
