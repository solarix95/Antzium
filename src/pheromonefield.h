#pragma once

#include <QPointF>
#include <QRectF>
#include <QVector>

class QPainter;
class WorldMap;

enum class PheromoneKind
{
    Food,
    Home
};

struct PheromoneCell
{
    double food = 0.0;
    double home = 0.0;
};

class PheromoneField
{
public:
    void reset(int widthTiles, int heightTiles, int tileSize);
    void clear();

    bool isValid() const;

    void deposit(PheromoneKind kind, const QPointF& worldPos, double amount);
    double sample(PheromoneKind kind, const QPointF& worldPos) const;

    void depositCircle(
        PheromoneKind kind,
        const QPointF& worldPos,
        double radiusPx,
        double amount,
        const WorldMap& map
        );

    void clearCircle(const QPointF& worldPos, double radiusPx);

    void evaporateAndDiffuse(double dt, const WorldMap& map);
    void draw(QPainter& painter, const QRectF& cameraWorldRect) const;

private:
    bool isInside(int tx, int ty) const;
    int index(int tx, int ty) const;

    double valueOf(const PheromoneCell& cell, PheromoneKind kind) const;
    void setValue(PheromoneCell& cell, PheromoneKind kind, double value);

private:
    int m_widthTiles = 0;
    int m_heightTiles = 0;
    int m_tileSize = 10;

    QVector<PheromoneCell> m_cells;
    QVector<PheromoneCell> m_buffer;
};
