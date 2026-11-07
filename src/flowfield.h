#pragma once

#include <QPoint>
#include <QPointF>
#include <QVector>

class WorldMap;

class FlowField
{
public:
    void rebuild(const WorldMap& map, const QPointF& goalWorldPos);
    void clear();

    bool isValid() const;

    QPointF directionAtWorld(const QPointF& worldPos) const;
    double costAtWorld(const QPointF& worldPos) const;
    bool isReachableWorld(const QPointF& worldPos) const;

private:
    struct Cell
    {
        double cost = 0.0;
        QPointF direction;
        bool reachable = false;
    };

    bool isInside(int tx, int ty) const;
    int index(int tx, int ty) const;

    QPoint worldToTile(const QPointF& worldPos) const;
    QPointF tileCenterWorld(int tx, int ty) const;

    bool diagonalAllowed(const WorldMap& map, int x, int y, int nx, int ny) const;
    double movementCost(const WorldMap& map, int fromX, int fromY, int toX, int toY) const;

private:
    int m_widthTiles = 0;
    int m_heightTiles = 0;
    int m_tileSize = 10;

    QVector<Cell> m_cells;
};
