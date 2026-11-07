#pragma once

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QVector>

#include "terrain.h"

class QPainter;

class WorldMap
{
public:
    bool loadFromPng(const QString& fileName, int tileSize);
    bool loadFromImage(const QImage& sourceImage, int tileSize);
    void createFallback(int widthTiles, int heightTiles, int tileSize);

    bool isValid() const;

    int widthTiles() const;
    int heightTiles() const;
    int tileSize() const;

    QSize worldSizePx() const;
    QColor tileRenderColor(int tx, int ty) const;

    bool isInsideTile(int tx, int ty) const;
    bool isWalkableWorld(const QPointF& worldPos) const;
    bool isWalkableTile(int tx, int ty) const;
    double speedAtWorld(const QPointF& worldPos) const;
    TerrainType terrainTypeAtWorld(const QPointF& worldPos) const;
    QColor renderColorAtWorld(const QPointF& worldPos) const;
    QImage toEditorImage() const;
    bool paintCircleTerrain(const QPointF& worldPos, double radiusPx, TerrainType type);
    bool clearCircleToGround(const QPointF& worldPos, double radiusPx);

    QPointF tileCenterWorld(int tx, int ty) const;

    const QVector<QPoint>& colonyStarts() const;
    const QVector<QColor>& colonyStartColors() const;
    const QVector<QPoint>& queenStarts() const;
    const QVector<QColor>& queenStartColors() const;
    const QVector<QPoint>& foodSpots() const;
    const QVector<QPoint>& spiderStarts() const;
    const QVector<QPoint>& flowerStarts() const;
    const QVector<QPoint>& spiderHoleStarts() const;

    void draw(QPainter& painter, const QRectF& cameraWorldRect) const;

private:
    Tile tileAt(int tx, int ty) const;

    static Tile tileFromColor(
        const QColor& color,
        bool* colonyStart,
        QColor* colonyColor,
        bool* queenStart,
        QColor* queenColor,
        bool* foodSpot,
        bool* spiderStart,
        bool* flowerStart,
        bool* spiderHoleStart
        );

    void rebuildTerrainSprites();
    QImage makeTerrainSprite(TerrainType type, int variant) const;
    const QImage* terrainSpriteForTile(const Tile& tile, int tx, int ty) const;

    int deterministicVariant(int tx, int ty, int count) const;
    static QColor shiftedColor(const QColor& base, int dr, int dg, int db);

private:
    int m_widthTiles = 0;
    int m_heightTiles = 0;
    int m_tileSize = 10;

    QVector<Tile> m_tiles;
    QVector<QColor> m_colonyStartColors;
    QVector<QPoint> m_colonyStarts;
    QVector<QColor> m_queenStartColors;
    QVector<QPoint> m_queenStarts;
    QVector<QPoint> m_foodSpots;
    QVector<QPoint> m_spiderStarts;
    QVector<QPoint> m_flowerStarts;
    QVector<QPoint> m_spiderHoleStarts;

    QVector<QImage> m_groundSprites;
    QVector<QImage> m_grassSprites;
    QVector<QImage> m_rockSprites;
    QVector<QImage> m_waterSprites;
    QVector<QImage> m_shallowWaterSprites;
};
