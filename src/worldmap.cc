#include "worldmap.h"

#include <QPainter>
#include <algorithm>
#include <cmath>


bool WorldMap::loadFromPng(const QString& fileName, int tileSize)
{
    QImage image;

    if (!image.load(fileName)) {
        return false;
    }

    return loadFromImage(image, tileSize);
}


bool WorldMap::loadFromImage(const QImage& sourceImage, int tileSize)
{
    if (sourceImage.isNull()) {
        return false;
    }

    QImage image = sourceImage.convertToFormat(QImage::Format_RGB32);

    m_widthTiles = image.width();
    m_heightTiles = image.height();
    m_tileSize = tileSize;

    m_tiles.clear();
    m_tiles.reserve(m_widthTiles * m_heightTiles);

    m_colonyStarts.clear();
    m_colonyStartColors.clear();
    m_queenStarts.clear();
    m_queenStartColors.clear();
    m_foodSpots.clear();
    m_spiderStarts.clear();
    m_flowerStarts.clear();
    m_spiderHoleStarts.clear();

    for (int y = 0; y < m_heightTiles; ++y) {
        for (int x = 0; x < m_widthTiles; ++x) {
            bool colony = false;
            QColor colonyColor;
            bool queen = false;
            QColor queenColor;

            bool food = false;
            bool spider = false;
            bool flower = false;
            bool spiderHole = false;

            Tile tile = tileFromColor(
                image.pixelColor(x, y),
                &colony,
                &colonyColor,
                &queen,
                &queenColor,
                &food,
                &spider,
                &flower,
                &spiderHole
                );

            m_tiles.push_back(tile);

            if (colony) {
                m_colonyStarts.push_back(QPoint(x, y));
                m_colonyStartColors.push_back(colonyColor);
            }

            if (queen) {
                m_queenStarts.push_back(QPoint(x, y));
                m_queenStartColors.push_back(queenColor);
            }

            if (food) {
                m_foodSpots.push_back(QPoint(x, y));
            }

            if (spider) {
                m_spiderStarts.push_back(QPoint(x, y));
            }

            if (flower) {
                m_flowerStarts.push_back(QPoint(x, y));
            }

            if (spiderHole) {
                m_spiderHoleStarts.push_back(QPoint(x, y));
            }
        }
    }

    rebuildTerrainSprites();

    return true;
}

void WorldMap::createFallback(int widthTiles, int heightTiles, int tileSize)
{
    m_widthTiles = widthTiles;
    m_heightTiles = heightTiles;
    m_tileSize = tileSize;

    m_tiles.clear();
    m_tiles.resize(m_widthTiles * m_heightTiles);

    m_colonyStarts.clear();
    m_colonyStartColors.clear();
    m_queenStarts.clear();
    m_queenStartColors.clear();
    m_foodSpots.clear();
    m_spiderStarts.clear();
    m_flowerStarts.clear();

    for (int y = 0; y < m_heightTiles; ++y) {
        for (int x = 0; x < m_widthTiles; ++x) {
            Tile tile;
            tile.type = TerrainType::Ground;
            tile.walkable = true;
            tile.speedFactor = 1.0;
            tile.renderColor = QColor(225, 210, 160);

            if (y > heightTiles / 3 && y < heightTiles / 2 && x > widthTiles / 4 && x < widthTiles - widthTiles / 5) {
                tile.type = TerrainType::Grass;
                tile.speedFactor = 0.45;
                tile.renderColor = QColor(80, 150, 70);
            }

            const double cx = widthTiles * 0.68;
            const double cy = heightTiles * 0.28;
            const double dx = (x - cx) / 20.0;
            const double dy = (y - cy) / 10.0;
            const double lake = dx * dx + dy * dy;

            if (lake < 1.0) {
                tile.type = TerrainType::Water;
                tile.walkable = false;
                tile.speedFactor = 0.0;
                tile.renderColor = QColor(36, 76, 158);
            }

            if (lake >= 1.0 && lake < 1.45) {
                tile.type = TerrainType::ShallowWater;
                tile.walkable = true;
                tile.speedFactor = 0.45;
                tile.renderColor = QColor(108, 191, 222);
            }

            if ((x > widthTiles / 2 - 4 && x < widthTiles / 2 + 4 && y > 10 && y < heightTiles - 10)
                || (x < 3 || y < 3 || x >= widthTiles - 3 || y >= heightTiles - 3)) {
                tile.type = TerrainType::Rock;
                tile.walkable = false;
                tile.speedFactor = 0.0;
                tile.renderColor = QColor(90, 90, 90);
            }

            m_tiles[y * m_widthTiles + x] = tile;
        }
    }

    m_colonyStarts.push_back(QPoint(12, heightTiles / 2));
    m_colonyStartColors.push_back(QColor(255, 0, 0));

    m_colonyStarts.push_back(QPoint(widthTiles - 12, heightTiles / 2));
    m_colonyStartColors.push_back(QColor(255, 0, 255));

    m_colonyStarts.push_back(QPoint(widthTiles / 3, heightTiles / 4));
    m_colonyStartColors.push_back(QColor(255, 128, 128));

    m_foodSpots.push_back(QPoint(widthTiles - 20, heightTiles / 2));
    m_foodSpots.push_back(QPoint(widthTiles - 25, heightTiles / 3));
    m_spiderStarts.push_back(QPoint(widthTiles * 2 / 3, heightTiles / 2));

    rebuildTerrainSprites();
}

bool WorldMap::isValid() const
{
    return m_widthTiles > 0 && m_heightTiles > 0 && !m_tiles.isEmpty();
}

int WorldMap::widthTiles() const
{
    return m_widthTiles;
}

int WorldMap::heightTiles() const
{
    return m_heightTiles;
}

int WorldMap::tileSize() const
{
    return m_tileSize;
}

QSize WorldMap::worldSizePx() const
{
    return QSize(m_widthTiles * m_tileSize, m_heightTiles * m_tileSize);
}

QColor WorldMap::tileRenderColor(int tx, int ty) const
{
    return tileAt(tx, ty).renderColor;
}

bool WorldMap::isInsideTile(int tx, int ty) const
{
    return tx >= 0 && ty >= 0 && tx < m_widthTiles && ty < m_heightTiles;
}

bool WorldMap::isWalkableWorld(const QPointF& worldPos) const
{
    const int tx = static_cast<int>(std::floor(worldPos.x() / m_tileSize));
    const int ty = static_cast<int>(std::floor(worldPos.y() / m_tileSize));

    if (!isInsideTile(tx, ty)) {
        return false;
    }

    return tileAt(tx, ty).walkable;
}

bool WorldMap::isWalkableTile(int tx, int ty) const
{
    if (!isInsideTile(tx, ty)) {
        return false;
    }

    return tileAt(tx, ty).walkable;
}

double WorldMap::speedAtWorld(const QPointF& worldPos) const
{
    const int tx = static_cast<int>(std::floor(worldPos.x() / m_tileSize));
    const int ty = static_cast<int>(std::floor(worldPos.y() / m_tileSize));

    if (!isInsideTile(tx, ty)) {
        return 0.0;
    }

    return tileAt(tx, ty).speedFactor;
}

TerrainType WorldMap::terrainTypeAtWorld(const QPointF& worldPos) const
{
    const int tx = static_cast<int>(std::floor(worldPos.x() / m_tileSize));
    const int ty = static_cast<int>(std::floor(worldPos.y() / m_tileSize));

    return tileAt(tx, ty).type;
}

QColor WorldMap::renderColorAtWorld(const QPointF& worldPos) const
{
    const int tx = static_cast<int>(std::floor(worldPos.x() / m_tileSize));
    const int ty = static_cast<int>(std::floor(worldPos.y() / m_tileSize));

    return tileAt(tx, ty).renderColor;
}

QImage WorldMap::toEditorImage() const
{
    if (!isValid()) {
        return QImage();
    }

    QImage image(m_widthTiles, m_heightTiles, QImage::Format_RGB32);

    for (int y = 0; y < m_heightTiles; ++y) {
        for (int x = 0; x < m_widthTiles; ++x) {
            const Tile tile = tileAt(x, y);
            QColor color(255, 255, 255);

            switch (tile.type) {
            case TerrainType::Grass:
                color = QColor(0, 200, 0);
                break;

            case TerrainType::Rock:
                color = QColor(100, 100, 100);
                break;

            case TerrainType::Water:
                color = QColor(0, 0, 255);
                break;

            case TerrainType::ShallowWater:
                color = QColor(102, 204, 255);
                break;

            case TerrainType::Ground:
            case TerrainType::ColonyStart:
            case TerrainType::Food:
            default:
                color = QColor(255, 255, 255);
                break;
            }

            image.setPixelColor(x, y, color);
        }
    }

    return image;
}

bool WorldMap::paintCircleTerrain(
    const QPointF& worldPos,
    double radiusPx,
    TerrainType type
    )
{
    if (!isValid() || radiusPx <= 0.0 || m_tileSize <= 0) {
        return false;
    }

    Tile tile;

    switch (type) {
    case TerrainType::Grass:
        tile.type = TerrainType::Grass;
        tile.walkable = true;
        tile.speedFactor = 0.45;
        tile.renderColor = QColor(80, 150, 70);
        break;

    case TerrainType::Rock:
        tile.type = TerrainType::Rock;
        tile.walkable = false;
        tile.speedFactor = 0.0;
        tile.renderColor = QColor(90, 90, 90);
        break;

    case TerrainType::Ground:
    default:
        tile.type = TerrainType::Ground;
        tile.walkable = true;
        tile.speedFactor = 1.0;
        tile.renderColor = QColor(225, 210, 160);
        break;
    }

    const int tx0 = std::max(
        0,
        static_cast<int>(std::floor((worldPos.x() - radiusPx) / m_tileSize))
        );

    const int ty0 = std::max(
        0,
        static_cast<int>(std::floor((worldPos.y() - radiusPx) / m_tileSize))
        );

    const int tx1 = std::min(
        m_widthTiles - 1,
        static_cast<int>(std::ceil((worldPos.x() + radiusPx) / m_tileSize))
        );

    const int ty1 = std::min(
        m_heightTiles - 1,
        static_cast<int>(std::ceil((worldPos.y() + radiusPx) / m_tileSize))
        );

    const double radiusSq = radiusPx * radiusPx;
    bool changed = false;

    for (int y = ty0; y <= ty1; ++y) {
        for (int x = tx0; x <= tx1; ++x) {
            const QPointF center = tileCenterWorld(x, y);

            const double dx = center.x() - worldPos.x();
            const double dy = center.y() - worldPos.y();

            if (dx * dx + dy * dy > radiusSq) {
                continue;
            }

            const int index = y * m_widthTiles + x;

            if (index < 0 || index >= m_tiles.size()) {
                continue;
            }

            const Tile& oldTile = m_tiles[index];

            if (oldTile.type != tile.type ||
                oldTile.walkable != tile.walkable ||
                std::abs(oldTile.speedFactor - tile.speedFactor) > 0.0001) {
                changed = true;
            }

            m_tiles[index] = tile;
        }
    }

    return changed;
}

bool WorldMap::clearCircleToGround(const QPointF& worldPos, double radiusPx)
{
    if (!isValid() || radiusPx <= 0.0 || m_tileSize <= 0) {
        return false;
    }

    const int tx0 = std::max(
        0,
        static_cast<int>(std::floor((worldPos.x() - radiusPx) / m_tileSize))
        );

    const int ty0 = std::max(
        0,
        static_cast<int>(std::floor((worldPos.y() - radiusPx) / m_tileSize))
        );

    const int tx1 = std::min(
        m_widthTiles - 1,
        static_cast<int>(std::ceil((worldPos.x() + radiusPx) / m_tileSize))
        );

    const int ty1 = std::min(
        m_heightTiles - 1,
        static_cast<int>(std::ceil((worldPos.y() + radiusPx) / m_tileSize))
        );

    const double radiusSq = radiusPx * radiusPx;

    Tile ground;
    ground.type = TerrainType::Ground;
    ground.walkable = true;
    ground.speedFactor = 1.0;
    ground.renderColor = QColor(225, 210, 160);

    bool changed = false;

    for (int y = ty0; y <= ty1; ++y) {
        for (int x = tx0; x <= tx1; ++x) {
            const QPointF center = tileCenterWorld(x, y);

            const double dx = center.x() - worldPos.x();
            const double dy = center.y() - worldPos.y();

            if (dx * dx + dy * dy > radiusSq) {
                continue;
            }

            const int index = y * m_widthTiles + x;

            if (index < 0 || index >= m_tiles.size()) {
                continue;
            }

            const Tile& oldTile = m_tiles[index];

            if (oldTile.type == TerrainType::Water ||
                oldTile.type == TerrainType::ShallowWater ||
                oldTile.type == TerrainType::ColonyStart) {
                continue;
            }

            if (oldTile.type != TerrainType::Ground ||
                !oldTile.walkable ||
                std::abs(oldTile.speedFactor - 1.0) > 0.0001) {
                changed = true;
            }

            m_tiles[index] = ground;
        }
    }

    return changed;
}

QPointF WorldMap::tileCenterWorld(int tx, int ty) const
{
    return QPointF(
        tx * m_tileSize + m_tileSize * 0.5,
        ty * m_tileSize + m_tileSize * 0.5
        );
}

const QVector<QPoint>& WorldMap::colonyStarts() const
{
    return m_colonyStarts;
}

const QVector<QColor>& WorldMap::colonyStartColors() const
{
    return m_colonyStartColors;
}

const QVector<QPoint>& WorldMap::queenStarts() const
{
    return m_queenStarts;
}

const QVector<QColor>& WorldMap::queenStartColors() const
{
    return m_queenStartColors;
}

const QVector<QPoint>& WorldMap::foodSpots() const
{
    return m_foodSpots;
}

const QVector<QPoint>& WorldMap::spiderStarts() const
{
    return m_spiderStarts;
}

const QVector<QPoint>& WorldMap::flowerStarts() const
{
    return m_flowerStarts;
}

const QVector<QPoint>& WorldMap::spiderHoleStarts() const
{
    return m_spiderHoleStarts;
}

void WorldMap::draw(QPainter& painter, const QRectF& cameraWorldRect) const
{
    if (!isValid()) {
        return;
    }

    const int x0 = std::max(0, static_cast<int>(std::floor(cameraWorldRect.left() / m_tileSize)));
    const int y0 = std::max(0, static_cast<int>(std::floor(cameraWorldRect.top() / m_tileSize)));

    const int x1 = std::min(m_widthTiles - 1, static_cast<int>(std::ceil(cameraWorldRect.right() / m_tileSize)));
    const int y1 = std::min(m_heightTiles - 1, static_cast<int>(std::ceil(cameraWorldRect.bottom() / m_tileSize)));

    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const Tile tile = tileAt(x, y);

            const QRectF rect(
                x * m_tileSize,
                y * m_tileSize,
                m_tileSize,
                m_tileSize
                );

            const QImage* sprite = terrainSpriteForTile(tile, x, y);

            if (sprite && !sprite->isNull()) {
                painter.drawImage(rect, *sprite);
            } else {
                painter.fillRect(rect, tile.renderColor);
            }
        }
    }
}

Tile WorldMap::tileAt(int tx, int ty) const
{
    static Tile outside {
        TerrainType::Rock,
        0.0,
        false,
        QColor(30, 30, 30)
    };

    if (!isInsideTile(tx, ty)) {
        return outside;
    }

    return m_tiles[ty * m_widthTiles + tx];
}

Tile WorldMap::tileFromColor(
    const QColor& color,
    bool* colonyStart,
    QColor* colonyColor,
    bool* queenStart,
    QColor* queenColor,
    bool* foodSpot,
    bool* spiderStart,
    bool* flowerStart,
    bool* spiderHoleStart
    )
{
    *colonyStart = false;
    *colonyColor = QColor(255, 0, 0);
    *queenStart = false;
    *queenColor = QColor(255, 0, 0);

    *foodSpot = false;
    *spiderStart = false;
    *flowerStart = false;
    *spiderHoleStart = false;

    const int r = color.red();
    const int g = color.green();
    const int b = color.blue();

    Tile tile;

    const bool isRed =
        r > 180 && g < 90 && b < 90;

    const bool isMagenta =
        r > 150 && b > 150 && g < 100;

    // Neue dritte Kolonie: #FF8080
    const bool isCoralColony =
        r > 210 &&
        g >= 90 && g <= 170 &&
        b >= 90 && b <= 170;

    const bool isYellow =
        r > 180 && g > 160 && b < 100;

    const bool isSpiderOrange =
        r > 210 && g > 70 && g < 170 && b < 90;

    const bool isFlowerViolet =
        b > 180 && r > 80 && r < 170 && g < 120;

    const bool isSpiderHole =
        r >= 55 && r <= 115 &&
        g >= 20 && g <= 70 &&
        b < 55;

    const bool isDeepWater =
        b > 180 && r < 80 && g < 120;

    const bool isShallowWater =
        b > 170 && g > 130 && r < 170;

    const bool isGreen =
        g > r + 25 && g > b + 25;

    const bool isGrey =
        std::abs(r - g) < 20 &&
        std::abs(g - b) < 20 &&
        r < 170;

    const bool isDark =
        r < 50 && g < 50 && b < 50;

    const bool isRedQueen =
        r == 128 && g == 0 && b == 0;

    const bool isMagentaQueen =
        r == 128 && g == 0 && b == 128;

    const bool isCoralQueen =
        r == 128 && g == 64 && b == 64;

    if (isRedQueen || isMagentaQueen || isCoralQueen) {
        *queenStart = true;

        if (isRedQueen) {
            *queenColor = QColor(255, 0, 0);
        } else if (isMagentaQueen) {
            *queenColor = QColor(255, 0, 255);
        } else {
            *queenColor = QColor(255, 128, 128);
        }

        tile.type = TerrainType::Ground;
        tile.walkable = true;
        tile.speedFactor = 1.0;
        tile.renderColor = QColor(225, 210, 160);
        return tile;
    }

    if (isRed || isMagenta || isCoralColony) {
        *colonyStart = true;

        if (isRed) {
            *colonyColor = QColor(255, 0, 0);
        } else if (isMagenta) {
            *colonyColor = QColor(255, 0, 255);
        } else {
            *colonyColor = QColor(255, 128, 128);
        }

        tile.type = TerrainType::ColonyStart;
        tile.walkable = true;
        tile.speedFactor = 1.0;
        tile.renderColor = QColor(215, 185, 135);
        return tile;
    }

    if (isYellow) {
        *foodSpot = true;
        tile.type = TerrainType::Food;
        tile.walkable = true;
        tile.speedFactor = 1.0;
        tile.renderColor = QColor(225, 210, 160);
        return tile;
    }

    if (isSpiderOrange) {
        *spiderStart = true;
        tile.type = TerrainType::Ground;
        tile.walkable = true;
        tile.speedFactor = 1.0;
        tile.renderColor = QColor(225, 210, 160);
        return tile;
    }

    if (isFlowerViolet) {
        *flowerStart = true;
        tile.type = TerrainType::Ground;
        tile.walkable = true;
        tile.speedFactor = 1.0;
        tile.renderColor = QColor(225, 210, 160);
        return tile;
    }

    if (isSpiderHole) {
        *spiderHoleStart = true;
        tile.type = TerrainType::Ground;
        tile.walkable = true;
        tile.speedFactor = 1.0;
        tile.renderColor = QColor(225, 210, 160);
        return tile;
    }

    if (isDeepWater) {
        tile.type = TerrainType::Water;
        tile.walkable = false;
        tile.speedFactor = 0.0;
        tile.renderColor = QColor(36, 76, 158);
        return tile;
    }

    if (isShallowWater) {
        tile.type = TerrainType::ShallowWater;
        tile.walkable = true;
        tile.speedFactor = 0.45;
        tile.renderColor = QColor(108, 191, 222);
        return tile;
    }

    if (isDark || isGrey) {
        tile.type = TerrainType::Rock;
        tile.walkable = false;
        tile.speedFactor = 0.0;
        tile.renderColor = QColor(90, 90, 90);
        return tile;
    }

    if (isGreen) {
        tile.type = TerrainType::Grass;
        tile.walkable = true;
        tile.speedFactor = 0.45;
        tile.renderColor = QColor(80, 150, 70);
        return tile;
    }

    tile.type = TerrainType::Ground;
    tile.walkable = true;
    tile.speedFactor = 1.0;
    tile.renderColor = QColor(225, 210, 160);
    return tile;
}

void WorldMap::rebuildTerrainSprites()
{
    constexpr int variants = 24;

    m_groundSprites.clear();
    m_grassSprites.clear();
    m_rockSprites.clear();
    m_waterSprites.clear();
    m_shallowWaterSprites.clear();

    m_groundSprites.reserve(variants);
    m_grassSprites.reserve(variants);
    m_rockSprites.reserve(variants);
    m_waterSprites.reserve(variants);
    m_shallowWaterSprites.reserve(variants);

    for (int i = 0; i < variants; ++i) {
        m_groundSprites.push_back(makeTerrainSprite(TerrainType::Ground, i));
        m_grassSprites.push_back(makeTerrainSprite(TerrainType::Grass, i));
        m_rockSprites.push_back(makeTerrainSprite(TerrainType::Rock, i));
        m_waterSprites.push_back(makeTerrainSprite(TerrainType::Water, i));
        m_shallowWaterSprites.push_back(makeTerrainSprite(TerrainType::ShallowWater, i));
    }
}

QImage WorldMap::makeTerrainSprite(TerrainType type, int variant) const
{
    const int size = std::max(2, m_tileSize);

    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);

    QColor base;

    switch (type) {
    case TerrainType::Grass:
        base = QColor(78, 145, 67);
        break;

    case TerrainType::Rock:
        base = QColor(92, 92, 90);
        break;

    case TerrainType::Water:
        base = QColor(36, 76, 158);
        break;

    case TerrainType::ShallowWater:
        base = QColor(108, 191, 222);
        break;

    case TerrainType::Ground:
    case TerrainType::ColonyStart:
    case TerrainType::Food:
    default:
        base = QColor(225, 210, 160);
        break;
    }

    quint32 seed =
        2166136261u
        ^ static_cast<quint32>(variant * 16777619u)
        ^ static_cast<quint32>(static_cast<int>(type) * 374761393u)
        ^ static_cast<quint32>(m_tileSize * 668265263u);

    auto rnd = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return seed;
    };

    auto range = [&rnd](int minValue, int maxValue) {
        const int span = maxValue - minValue + 1;
        return minValue + static_cast<int>(rnd() % static_cast<quint32>(span));
    };

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            QColor c;

            switch (type) {
            case TerrainType::Grass:
                c = shiftedColor(
                    base,
                    range(-10, 8),
                    range(-14, 18),
                    range(-10, 8)
                    );
                break;

            case TerrainType::Rock:
                c = shiftedColor(
                    base,
                    range(-18, 16),
                    range(-18, 16),
                    range(-18, 16)
                    );
                break;

            case TerrainType::Water:
                c = shiftedColor(
                    base,
                    range(-8, 8),
                    range(-10, 12),
                    range(-18, 24)
                    );
                break;

            case TerrainType::ShallowWater:
                c = shiftedColor(
                    base,
                    range(-10, 10),
                    range(-12, 12),
                    range(-14, 18)
                    );
                break;

            case TerrainType::Ground:
            case TerrainType::ColonyStart:
            case TerrainType::Food:
            default:
                c = shiftedColor(
                    base,
                    range(-10, 12),
                    range(-9, 9),
                    range(-8, 7)
                    );
                break;
            }

            image.setPixelColor(x, y, c);
        }
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);

    if (type == TerrainType::Ground || type == TerrainType::ColonyStart || type == TerrainType::Food) {
        const int pebbleCount = range(1, 4);

        for (int i = 0; i < pebbleCount; ++i) {
            const int x = range(0, size - 1);
            const int y = range(0, size - 1);

            painter.setPen(shiftedColor(base, range(-35, -18), range(-30, -15), range(-22, -10)));
            painter.drawPoint(x, y);

            if (size >= 8 && range(0, 3) == 0) {
                painter.setPen(shiftedColor(base, range(10, 22), range(8, 18), range(4, 12)));
                painter.drawPoint(std::min(size - 1, x + 1), y);
            }
        }
    }

    if (type == TerrainType::Grass) {
        const int bladeCount = range(3, 7);

        for (int i = 0; i < bladeCount; ++i) {
            const int x = range(0, size - 1);
            const int y = range(0, size - 1);
            const int len = range(2, std::max(2, size / 2));

            const QColor grassLine = shiftedColor(base, range(-20, 8), range(8, 35), range(-20, 5));
            painter.setPen(grassLine);

            painter.drawLine(
                x,
                y,
                std::clamp(x + range(-1, 1), 0, size - 1),
                std::clamp(y - len, 0, size - 1)
                );
        }

        if (range(0, 4) == 0) {
            painter.setPen(shiftedColor(base, -22, -18, -16));
            painter.drawPoint(range(0, size - 1), range(0, size - 1));
        }
    }

    if (type == TerrainType::Rock) {
        const int crackCount = range(1, 3);

        for (int i = 0; i < crackCount; ++i) {
            const int x = range(0, size - 1);
            const int y = range(0, size - 1);
            const int x2 = std::clamp(x + range(-3, 3), 0, size - 1);
            const int y2 = std::clamp(y + range(-3, 3), 0, size - 1);

            painter.setPen(shiftedColor(base, -32, -32, -32));
            painter.drawLine(x, y, x2, y2);
        }

        const int highlightCount = range(1, 3);

        for (int i = 0; i < highlightCount; ++i) {
            painter.setPen(shiftedColor(base, 28, 28, 26));
            painter.drawPoint(range(0, size - 1), range(0, size - 1));
        }
    }

    if (type == TerrainType::Water || type == TerrainType::ShallowWater) {
        const int waveCount = type == TerrainType::Water
                                  ? range(1, 3)
                                  : range(2, 4);

        for (int i = 0; i < waveCount; ++i) {
            const int y = range(1, size - 2);
            const int x0 = range(0, std::max(0, size / 2));
            const int len = range(3, std::max(3, size - x0));

            const QColor waveColor = type == TerrainType::Water
                                         ? shiftedColor(base, 20, 30, 45)
                                         : shiftedColor(base, 35, 35, 35);

            painter.setPen(waveColor);

            for (int x = x0; x < std::min(size - 1, x0 + len); ++x) {
                const int yy = std::clamp(y + ((x + variant) % 3 == 0 ? 1 : 0), 0, size - 1);
                painter.drawPoint(x, yy);
            }
        }

        if (type == TerrainType::ShallowWater) {
            const int sandHints = range(1, 3);

            for (int i = 0; i < sandHints; ++i) {
                painter.setPen(QColor(210, 205, 150, 120));
                painter.drawPoint(range(0, size - 1), range(0, size - 1));
            }
        }
    }

    painter.end();

    return image;
}

int WorldMap::deterministicVariant(int tx, int ty, int count) const
{
    if (count <= 0) {
        return 0;
    }

    quint32 h = 2166136261u;

    h ^= static_cast<quint32>(tx);
    h *= 16777619u;

    h ^= static_cast<quint32>(ty);
    h *= 16777619u;

    h ^= static_cast<quint32>(tx * 73856093);
    h ^= static_cast<quint32>(ty * 19349663);

    return static_cast<int>(h % static_cast<quint32>(count));
}

const QImage* WorldMap::terrainSpriteForTile(const Tile& tile, int tx, int ty) const
{
    const QVector<QImage>* sprites = nullptr;

    switch (tile.type) {
    case TerrainType::Grass:
        sprites = &m_grassSprites;
        break;

    case TerrainType::Rock:
        sprites = &m_rockSprites;
        break;

    case TerrainType::Water:
        sprites = &m_waterSprites;
        break;

    case TerrainType::ShallowWater:
        sprites = &m_shallowWaterSprites;
        break;

    case TerrainType::Ground:
    case TerrainType::ColonyStart:
    case TerrainType::Food:
    default:
        sprites = &m_groundSprites;
        break;
    }

    if (!sprites || sprites->isEmpty()) {
        return nullptr;
    }

    const int variant = deterministicVariant(tx, ty, sprites->size());
    return &sprites->at(variant);
}

QColor WorldMap::shiftedColor(const QColor& base, int dr, int dg, int db)
{
    return QColor(
        std::clamp(base.red() + dr, 0, 255),
        std::clamp(base.green() + dg, 0, 255),
        std::clamp(base.blue() + db, 0, 255),
        base.alpha()
        );
}
