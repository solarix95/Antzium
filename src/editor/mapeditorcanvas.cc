#include "mapeditorcanvas.h"

#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QVector>
#include <QRect>

#include <algorithm>
#include <cmath>

namespace
{
const QColor GroundColor       = QColor(255, 255, 255); // #FFFFFF
const QColor GrassColor        = QColor(0, 200, 0);     // #00C800
const QColor RockColor         = QColor(100, 100, 100); // #646464
const QColor WaterColor        = QColor(0, 0, 255);     // #0000FF
const QColor ShallowWaterColor = QColor(102, 204, 255); // #66CCFF
const QColor FoodColor         = QColor(255, 255, 0);   // #FFFF00
const QColor SpiderColor       = QColor(255, 128, 0);   // #FF8000
const QColor FlowerColor       = QColor(128, 0, 255); // #8000FF
const QColor SpiderHoleColor   = QColor(80, 40, 20); // #502814
const QColor ColonyRedColor    = QColor(255, 0, 0);     // #FF0000
const QColor ColonyPinkColor   = QColor(255, 0, 255);   // #FF00FF
const QColor ColonyCoralColor = QColor(255, 128, 128); // #FF8080
const QColor QueenRedColor     = QColor(128, 0, 0);     // #800000
const QColor QueenPinkColor    = QColor(128, 0, 128);   // #800080
const QColor QueenCoralColor   = QColor(128, 64, 64);   // #804040
}

MapEditorCanvas::MapEditorCanvas(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(640, 420);

    createBlankMap(QSize(256, 144));
}

const QImage& MapEditorCanvas::image() const
{
    return m_image;
}

void MapEditorCanvas::setImage(const QImage& image)
{
    if (image.isNull()) {
        return;
    }

    m_image = image.convertToFormat(QImage::Format_RGB32);
    update();

    emit imageChanged();
}

void MapEditorCanvas::createBlankMap(const QSize& size)
{
    m_image = QImage(size, QImage::Format_RGB32);
    m_image.fill(GroundColor);

    // Eine rote Startposition als Default.
    m_image.setPixelColor(size.width() / 4, size.height() / 2, ColonyRedColor);

    update();
    emit imageChanged();
}

void MapEditorCanvas::randomize()
{
    if (m_image.isNull()) {
        createBlankMap(QSize(256, 144));
    }

    m_image.fill(GroundColor);

    const QSize size = m_image.size();
    const int w = size.width();
    const int h = size.height();

    auto rndInt = [](int minValue, int maxValue) {
        if (maxValue <= minValue) {
            return minValue;
        }

        return QRandomGenerator::global()->bounded(minValue, maxValue + 1);
    };

    const double areaScale =
        static_cast<double>(w * h) / static_cast<double>(256 * 144);

    // ------------------------------------------------------------
    // Große Wiesenflächen
    // ------------------------------------------------------------

    const int grassBlobCount =
        std::clamp(static_cast<int>(7 * areaScale), 5, 16);

    for (int i = 0; i < grassBlobCount; ++i) {
        const QPoint center(
            rndInt(0, w - 1),
            rndInt(0, h - 1)
            );

        const int rx = rndInt(w / 14, w / 5);
        const int ry = rndInt(h / 18, h / 6);

        drawOrganicBlob(
            m_image,
            center,
            rx,
            ry,
            GrassColor,
            rndInt(1, 3)
            );
    }

    // ------------------------------------------------------------
    // Steinriegel / Felsgruppen
    // ------------------------------------------------------------

    const int ridgeCount =
        std::clamp(static_cast<int>(4 * areaScale), 2, 10);

    for (int i = 0; i < ridgeCount; ++i) {
        const QPoint start(
            rndInt(0, w - 1),
            rndInt(0, h - 1)
            );

        const QPoint end(
            std::clamp(start.x() + rndInt(-w / 3, w / 3), 0, w - 1),
            std::clamp(start.y() + rndInt(-h / 3, h / 3), 0, h - 1)
            );

        drawRockRidge(
            m_image,
            start,
            end,
            rndInt(2, std::max(3, w / 55))
            );
    }

    // Einzelne Felsinseln
    const int rockBlobCount =
        std::clamp(static_cast<int>(5 * areaScale), 3, 12);

    for (int i = 0; i < rockBlobCount; ++i) {
        drawOrganicBlob(
            m_image,
            QPoint(rndInt(0, w - 1), rndInt(0, h - 1)),
            rndInt(3, std::max(5, w / 18)),
            rndInt(3, std::max(5, h / 16)),
            RockColor,
            rndInt(1, 2)
            );
    }

    // ------------------------------------------------------------
    // Seen
    // ------------------------------------------------------------

    const int lakeCount =
        rndInt(0, std::clamp(static_cast<int>(2 * areaScale), 1, 4));

    for (int i = 0; i < lakeCount; ++i) {
        drawLake(
            m_image,
            QPoint(rndInt(w / 8, w - w / 8), rndInt(h / 8, h - h / 8)),
            rndInt(w / 18, w / 9),
            rndInt(h / 20, h / 9)
            );
    }

    // ------------------------------------------------------------
    // Fluss
    // ------------------------------------------------------------

    const int riverChance = w >= 180 ? 70 : 45;

    if (rndInt(1, 100) <= riverChance) {
        drawMeanderingRiver(m_image);
    }

    // ------------------------------------------------------------
    // Noch ein paar kleine Untiefen / Feuchtstellen
    // ------------------------------------------------------------

    const int shallowPondCount =
        std::clamp(static_cast<int>(3 * areaScale), 1, 7);

    for (int i = 0; i < shallowPondCount; ++i) {
        drawOrganicBlob(
            m_image,
            QPoint(rndInt(0, w - 1), rndInt(0, h - 1)),
            rndInt(4, std::max(5, w / 30)),
            rndInt(3, std::max(4, h / 28)),
            ShallowWaterColor,
            1
            );
    }

    // ------------------------------------------------------------
    // Kolonien, Futter, Spinnen auf begehbarem Terrain
    // ------------------------------------------------------------

    QVector<QPoint> reserved;

    const QRect leftArea(0, 0, w / 2, h);
    const QRect rightArea(w / 2, 0, w - w / 2, h);

    const QPoint red = randomPointInRectOnGroundLike(leftArea, reserved, std::max(10, w / 8));
    reserved.push_back(red);
    m_image.setPixelColor(red, ColonyRedColor);

    const QPoint pink = randomPointInRectOnGroundLike(rightArea, reserved, std::max(10, w / 8));
    reserved.push_back(pink);
    m_image.setPixelColor(pink, ColonyPinkColor);

    const int flowerCount =
        std::clamp(static_cast<int>(8 * areaScale), 4, 24);

    for (int i = 0; i < flowerCount; ++i) {
        const QPoint p = randomPointOnGroundLike(reserved, std::max(4, w / 32));
        reserved.push_back(p);
        m_image.setPixelColor(p, FlowerColor);
    }

    const int spiderHoleCount =
        std::clamp(static_cast<int>(2 * areaScale), 1, 6);

    for (int i = 0; i < spiderHoleCount; ++i) {
        const QPoint p = randomPointOnGroundLike(reserved, std::max(8, w / 18));
        reserved.push_back(p);
        m_image.setPixelColor(p, SpiderHoleColor);
    }

    const int foodCount =
        std::clamp(static_cast<int>(8 * areaScale), 6, 24);

    for (int i = 0; i < foodCount; ++i) {
        const QPoint p = randomPointOnGroundLike(reserved, std::max(5, w / 28));
        reserved.push_back(p);
        m_image.setPixelColor(p, FoodColor);
    }

    const int spiderCount =
        std::clamp(static_cast<int>(3 * areaScale), 1, 10);

    for (int i = 0; i < spiderCount; ++i) {
        const QPoint p = randomPointOnGroundLike(reserved, std::max(6, w / 24));
        reserved.push_back(p);
        m_image.setPixelColor(p, SpiderColor);
    }

    update();
    emit imageChanged();
}

void MapEditorCanvas::setTool(Tool tool)
{
    m_tool = tool;
}

void MapEditorCanvas::paintEvent(QPaintEvent*)
{
    QPainter painter(this);

    painter.fillRect(rect(), QColor(18, 20, 23));

    if (m_image.isNull()) {
        return;
    }

    const QRectF target = mapTargetRect();

    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(target, m_image);

    painter.setPen(QColor(80, 90, 100));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(target.adjusted(0.5, 0.5, -0.5, -0.5));
}

void MapEditorCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        paintAtWidgetPos(event->pos());
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void MapEditorCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        paintAtWidgetPos(event->pos());
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

QRectF MapEditorCanvas::mapTargetRect() const
{
    QRectF box = QRectF(rect()).adjusted(8.0, 8.0, -8.0, -8.0);

    if (m_image.isNull()) {
        return box;
    }

    const double mapW = std::max(1, m_image.width());
    const double mapH = std::max(1, m_image.height());

    const double boxRatio = box.width() / box.height();
    const double mapRatio = mapW / mapH;

    double w = box.width();
    double h = box.height();

    if (mapRatio > boxRatio) {
        h = w / mapRatio;
    } else {
        w = h * mapRatio;
    }

    return QRectF(
        box.center().x() - w * 0.5,
        box.center().y() - h * 0.5,
        w,
        h
        );
}

QPoint MapEditorCanvas::widgetToImagePixel(const QPoint& pos) const
{
    const QRectF target = mapTargetRect();

    if (!target.contains(pos) || m_image.isNull()) {
        return QPoint(-1, -1);
    }

    const double nx = (pos.x() - target.left()) / target.width();
    const double ny = (pos.y() - target.top()) / target.height();

    const int x = std::clamp(static_cast<int>(nx * m_image.width()), 0, m_image.width() - 1);
    const int y = std::clamp(static_cast<int>(ny * m_image.height()), 0, m_image.height() - 1);

    return QPoint(x, y);
}

void MapEditorCanvas::paintAtWidgetPos(const QPoint& pos)
{
    const QPoint pixel = widgetToImagePixel(pos);

    if (pixel.x() < 0 || pixel.y() < 0) {
        return;
    }

    paintPixel(pixel.x(), pixel.y());

    update();
    emit imageChanged();
}

void MapEditorCanvas::paintPixel(int x, int y)
{
    if (m_image.isNull()) {
        return;
    }

    if (x < 0 || y < 0 || x >= m_image.width() || y >= m_image.height()) {
        return;
    }

    const QColor color = colorForTool(m_tool);

    if (m_tool == Tool::ColonyRed ||
        m_tool == Tool::ColonyMagenta ||
        m_tool == Tool::ColonyCoral ||
        m_tool == Tool::QueenRed ||
        m_tool == Tool::QueenMagenta ||
        m_tool == Tool::QueenCoral)
    {
        clearExistingMarkerColor(color);
        m_image.setPixelColor(x, y, color);
        return;
    }

    if (isTerrainTool(m_tool)) {
        paintTerrainBrush(x, y, color);
        return;
    }

    // Marker wie Nahrung und Spinne bleiben bewusst punktgenau.
    m_image.setPixelColor(x, y, color);
}

QPoint MapEditorCanvas::randomPointOnGroundLike(
    const QVector<QPoint>& avoid,
    int minDistanceTiles
    ) const
{
    if (m_image.isNull()) {
        return QPoint(0, 0);
    }

    const int minDistSq = minDistanceTiles * minDistanceTiles;

    for (int i = 0; i < 1200; ++i) {
        const int x = QRandomGenerator::global()->bounded(m_image.width());
        const int y = QRandomGenerator::global()->bounded(m_image.height());

        const QColor c = m_image.pixelColor(x, y);

        if (!isGroundLikeColor(c)) {
            continue;
        }

        bool tooClose = false;

        for (const QPoint& p : avoid) {
            const int dx = p.x() - x;
            const int dy = p.y() - y;

            if (dx * dx + dy * dy < minDistSq) {
                tooClose = true;
                break;
            }
        }

        if (!tooClose) {
            return QPoint(x, y);
        }
    }

    return QPoint(m_image.width() / 2, m_image.height() / 2);
}

QPoint MapEditorCanvas::randomPointInRectOnGroundLike(
    const QRect& area,
    const QVector<QPoint>& avoid,
    int minDistanceTiles
    ) const
{
    if (m_image.isNull()) {
        return QPoint(0, 0);
    }

    const QRect clampedArea =
        area.intersected(QRect(0, 0, m_image.width(), m_image.height()));

    if (clampedArea.isEmpty()) {
        return randomPointOnGroundLike(avoid, minDistanceTiles);
    }

    const int minDistSq = minDistanceTiles * minDistanceTiles;

    for (int i = 0; i < 1200; ++i) {
        const int x = QRandomGenerator::global()->bounded(
            clampedArea.left(),
            clampedArea.right() + 1
            );

        const int y = QRandomGenerator::global()->bounded(
            clampedArea.top(),
            clampedArea.bottom() + 1
            );

        const QColor c = m_image.pixelColor(x, y);

        if (!isGroundLikeColor(c)) {
            continue;
        }

        bool tooClose = false;

        for (const QPoint& p : avoid) {
            const int dx = p.x() - x;
            const int dy = p.y() - y;

            if (dx * dx + dy * dy < minDistSq) {
                tooClose = true;
                break;
            }
        }

        if (!tooClose) {
            return QPoint(x, y);
        }
    }

    return randomPointOnGroundLike(avoid, minDistanceTiles);
}

void MapEditorCanvas::paintTerrainBrush(int cx, int cy, const QColor& color)
{
    const int r = m_terrainBrushRadius;
    const int rr = r * r;

    for (int y = cy - r; y <= cy + r; ++y) {
        for (int x = cx - r; x <= cx + r; ++x) {
            if (x < 0 || y < 0 || x >= m_image.width() || y >= m_image.height()) {
                continue;
            }

            const int dx = x - cx;
            const int dy = y - cy;

            if (dx * dx + dy * dy <= rr) {
                m_image.setPixelColor(x, y, color);
            }
        }
    }
}

void MapEditorCanvas::clearExistingMarkerColor(const QColor& color)
{
    for (int y = 0; y < m_image.height(); ++y) {
        for (int x = 0; x < m_image.width(); ++x) {
            if (m_image.pixelColor(x, y).rgb() == color.rgb()) {
                m_image.setPixelColor(x, y, GroundColor);
            }
        }
    }
}

QColor MapEditorCanvas::colorForTool(Tool tool) const
{
    switch (tool) {
    case Tool::Ground:
        return GroundColor;

    case Tool::Grass:
        return GrassColor;

    case Tool::Rock:
        return RockColor;

    case Tool::Water:
        return WaterColor;

    case Tool::ShallowWater:
        return ShallowWaterColor;

    case Tool::Food:
        return FoodColor;

    case Tool::Spider:
        return SpiderColor;

    case Tool::Flower:
        return FlowerColor;

    case Tool::SpiderHole:
        return SpiderHoleColor;

    case Tool::ColonyRed:
        return ColonyRedColor;

    case Tool::ColonyMagenta:
        return ColonyPinkColor;

    case Tool::ColonyCoral:
        return ColonyCoralColor;

    case Tool::QueenRed:
        return QueenRedColor;

    case Tool::QueenMagenta:
        return QueenPinkColor;

    case Tool::QueenCoral:
        return QueenCoralColor;
    }

    return GroundColor;
}

bool MapEditorCanvas::isTerrainTool(Tool tool) const
{
    return tool == Tool::Ground
           || tool == Tool::Grass
           || tool == Tool::Rock
           || tool == Tool::Water
           || tool == Tool::ShallowWater;
}

void MapEditorCanvas::drawOrganicBlob(
    QImage& img,
    const QPoint& center,
    int rx,
    int ry,
    const QColor& color,
    int passes
    )
{
    if (img.isNull()) {
        return;
    }

    auto rndInt = [](int minValue, int maxValue) {
        if (maxValue <= minValue) {
            return minValue;
        }

        return QRandomGenerator::global()->bounded(minValue, maxValue + 1);
    };

    passes = std::max(1, passes);

    for (int pass = 0; pass < passes; ++pass) {
        const QPoint c(
            std::clamp(center.x() + rndInt(-rx / 4, rx / 4), 0, img.width() - 1),
            std::clamp(center.y() + rndInt(-ry / 4, ry / 4), 0, img.height() - 1)
            );

        const int prx = std::max(1, rx + rndInt(-rx / 5, rx / 5));
        const int pry = std::max(1, ry + rndInt(-ry / 5, ry / 5));

        for (int y = c.y() - pry; y <= c.y() + pry; ++y) {
            for (int x = c.x() - prx; x <= c.x() + prx; ++x) {
                if (x < 0 || y < 0 || x >= img.width() || y >= img.height()) {
                    continue;
                }

                const double dx = static_cast<double>(x - c.x()) / prx;
                const double dy = static_cast<double>(y - c.y()) / pry;
                const double d = dx * dx + dy * dy;

                int chance = 0;

                if (d < 0.62) {
                    chance = 100;
                } else if (d < 0.88) {
                    chance = 82;
                } else if (d < 1.08) {
                    chance = 38;
                } else if (d < 1.22) {
                    chance = 12;
                }

                if (chance > 0 && rndInt(1, 100) <= chance) {
                    img.setPixelColor(x, y, color);
                }
            }
        }
    }
}


void MapEditorCanvas::drawDisc(
    QImage& img,
    const QPoint& center,
    int radius,
    const QColor& color,
    int chancePercent
    )
{
    if (img.isNull()) {
        return;
    }

    radius = std::max(1, radius);
    chancePercent = std::clamp(chancePercent, 0, 100);

    const int rr = radius * radius;

    for (int y = center.y() - radius; y <= center.y() + radius; ++y) {
        for (int x = center.x() - radius; x <= center.x() + radius; ++x) {
            if (x < 0 || y < 0 || x >= img.width() || y >= img.height()) {
                continue;
            }

            const int dx = x - center.x();
            const int dy = y - center.y();

            if (dx * dx + dy * dy > rr) {
                continue;
            }

            if (QRandomGenerator::global()->bounded(100) < chancePercent) {
                img.setPixelColor(x, y, color);
            }
        }
    }
}

void MapEditorCanvas::drawLake(QImage& img, const QPoint& center, int rx, int ry)
{
    // Untiefer Rand
    drawOrganicBlob(
        img,
        center,
        rx + std::max(2, rx / 4),
        ry + std::max(2, ry / 4),
        ShallowWaterColor,
        2
        );

    // Tiefes Wasser innen
    drawOrganicBlob(
        img,
        center,
        rx,
        ry,
        WaterColor,
        2
        );

    // Kleine unregelmäßige Ufer-Auflockerung
    for (int i = 0; i < 4; ++i) {
        const double angle =
            QRandomGenerator::global()->generateDouble() * 6.28318530718;

        const QPoint p(
            center.x() + static_cast<int>(std::cos(angle) * rx * 0.9),
            center.y() + static_cast<int>(std::sin(angle) * ry * 0.9)
            );

        drawOrganicBlob(
            img,
            p,
            std::max(2, rx / 4),
            std::max(2, ry / 4),
            ShallowWaterColor,
            1
            );
    }
}

void MapEditorCanvas::drawMeanderingRiver(QImage& img)
{
    if (img.isNull()) {
        return;
    }

    const int w = img.width();
    const int h = img.height();

    auto rndInt = [](int minValue, int maxValue) {
        if (maxValue <= minValue) {
            return minValue;
        }

        return QRandomGenerator::global()->bounded(minValue, maxValue + 1);
    };

    const bool horizontal =
        QRandomGenerator::global()->bounded(2) == 0;

    const int shallowRadius =
        std::clamp(horizontal ? h / 22 : w / 22, 4, 14);

    const int deepRadius =
        std::max(2, static_cast<int>(shallowRadius * 0.55));

    double drift = 0.0;

    if (horizontal) {
        double y = rndInt(h / 5, h - h / 5);

        for (int x = -shallowRadius; x < w + shallowRadius; x += 2) {
            drift += (QRandomGenerator::global()->generateDouble() - 0.5) * 0.9;
            drift = std::clamp(drift, -2.2, 2.2);

            y += drift;
            y = std::clamp(y, static_cast<double>(shallowRadius + 2), static_cast<double>(h - shallowRadius - 3));

            const QPoint c(x, static_cast<int>(y));

            drawDisc(img, c, shallowRadius, ShallowWaterColor, 100);
            drawDisc(img, c, deepRadius, WaterColor, 96);

            // Leichte Ausbuchtungen
            if (rndInt(1, 100) < 8) {
                const QPoint side(
                    x,
                    static_cast<int>(y + rndInt(-shallowRadius, shallowRadius))
                    );

                drawDisc(img, side, shallowRadius / 2, ShallowWaterColor, 80);
            }
        }
    } else {
        double x = rndInt(w / 5, w - w / 5);

        for (int y = -shallowRadius; y < h + shallowRadius; y += 2) {
            drift += (QRandomGenerator::global()->generateDouble() - 0.5) * 0.9;
            drift = std::clamp(drift, -2.2, 2.2);

            x += drift;
            x = std::clamp(x, static_cast<double>(shallowRadius + 2), static_cast<double>(w - shallowRadius - 3));

            const QPoint c(static_cast<int>(x), y);

            drawDisc(img, c, shallowRadius, ShallowWaterColor, 100);
            drawDisc(img, c, deepRadius, WaterColor, 96);

            if (rndInt(1, 100) < 8) {
                const QPoint side(
                    static_cast<int>(x + rndInt(-shallowRadius, shallowRadius)),
                    y
                    );

                drawDisc(img, side, shallowRadius / 2, ShallowWaterColor, 80);
            }
        }
    }
}

void MapEditorCanvas::drawRockRidge(
    QImage& img,
    const QPoint& start,
    const QPoint& end,
    int width
    )
{
    if (img.isNull()) {
        return;
    }

    const int steps =
        std::max(std::abs(end.x() - start.x()), std::abs(end.y() - start.y()));

    if (steps <= 0) {
        return;
    }

    auto rndInt = [](int minValue, int maxValue) {
        if (maxValue <= minValue) {
            return minValue;
        }

        return QRandomGenerator::global()->bounded(minValue, maxValue + 1);
    };

    for (int i = 0; i <= steps; i += 2) {
        const double t = static_cast<double>(i) / steps;

        QPoint p(
            static_cast<int>(start.x() * (1.0 - t) + end.x() * t),
            static_cast<int>(start.y() * (1.0 - t) + end.y() * t)
            );

        p.rx() += rndInt(-2, 2);
        p.ry() += rndInt(-2, 2);

        const int r = std::max(1, width + rndInt(-1, 2));

        drawDisc(img, p, r, RockColor, 78);
    }

    // Ein paar dickere Felsbrocken entlang des Riegels
    for (int i = 0; i < 4; ++i) {
        const double t = QRandomGenerator::global()->generateDouble();

        QPoint p(
            static_cast<int>(start.x() * (1.0 - t) + end.x() * t),
            static_cast<int>(start.y() * (1.0 - t) + end.y() * t)
            );

        drawOrganicBlob(
            img,
            p,
            width * 2 + rndInt(1, 5),
            width * 2 + rndInt(1, 5),
            RockColor,
            1
            );
    }
}

bool MapEditorCanvas::isGroundLikeColor(const QColor& color) const
{
    return color.rgb() == GroundColor.rgb()
           || color.rgb() == GrassColor.rgb()
           || color.rgb() == ShallowWaterColor.rgb();
}
