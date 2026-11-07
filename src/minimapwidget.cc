#include "minimapwidget.h"

#include "gamewidget.h"
#include "gameworld.h"
#include "worldmap.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPen>

#include <algorithm>

MiniMapWidget::MiniMapWidget(GameWidget* game, QWidget* parent)
    : QWidget(parent)
    , m_game(game)
{
    setMouseTracking(true);

    m_terrainRevisionTimer.setInterval(5000);
    m_terrainRevisionTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_terrainRevisionTimer, &QTimer::timeout, this, [this]() {
        if (!m_game || !m_game->world().hasMap()) {
            return;
        }

        if (m_terrainRevision == m_game->world().terrainRevision()) {
            return;
        }

        m_dirty = true;
        update();
    });
    m_terrainRevisionTimer.start();

    if (m_game) {
        connect(m_game, &GameWidget::worldChanged, this, [this]() {
            m_dirty = true;
            update();
        });

        connect(m_game, &GameWidget::viewChanged, this, [this]() {
            update();
        });
    }
}

QSize MiniMapWidget::minimumSizeHint() const
{
    return QSize(160, 110);
}

void MiniMapWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);

    painter.fillRect(rect(), QColor(12, 15, 18));

    if (!m_game || !m_game->world().hasMap()) {
        painter.setPen(QColor(220, 220, 220));
        painter.drawText(rect(), Qt::AlignCenter, "Keine Map");
        return;
    }

    if (m_dirty || m_mapImage.isNull()) {
        rebuildMapImage();
    }

    const QRectF target = mapTargetRect();

    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(target, m_mapImage);

    painter.setPen(QPen(QColor(70, 80, 90), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(target.adjusted(0.5, 0.5, -0.5, -0.5));

    const WorldMap& map = m_game->world().map();
    const QSize worldSize = map.worldSizePx();

    if (worldSize.width() <= 0 || worldSize.height() <= 0) {
        return;
    }

    const QRectF camera = m_game->cameraWorldRect();

    const double sx = target.width() / worldSize.width();
    const double sy = target.height() / worldSize.height();

    QRectF viewRect(
        target.left() + camera.left() * sx,
        target.top() + camera.top() * sy,
        camera.width() * sx,
        camera.height() * sy
        );

    painter.setPen(QPen(Qt::white, 2));
    painter.setBrush(QColor(255, 255, 255, 24));
    painter.drawRect(viewRect);
}

void MiniMapWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        centerCameraAtWidgetPos(event->pos());
    }

    QWidget::mousePressEvent(event);
}

void MiniMapWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        centerCameraAtWidgetPos(event->pos());
    }

    QWidget::mouseMoveEvent(event);
}

void MiniMapWidget::rebuildMapImage()
{
    m_dirty = false;
    m_mapImage = QImage();

    if (!m_game || !m_game->world().hasMap()) {
        return;
    }

    const WorldMap& map = m_game->world().map();
    m_terrainRevision = m_game->world().terrainRevision();

    QImage image(map.widthTiles(), map.heightTiles(), QImage::Format_RGB32);

    for (int y = 0; y < map.heightTiles(); ++y) {
        for (int x = 0; x < map.widthTiles(); ++x) {
            image.setPixelColor(x, y, map.tileRenderColor(x, y));
        }
    }

    m_mapImage = image;
}

QRectF MiniMapWidget::mapTargetRect() const
{
    QRectF box = QRectF(rect()).adjusted(8.0, 8.0, -8.0, -8.0);

    if (!m_game || !m_game->world().hasMap()) {
        return box;
    }

    const WorldMap& map = m_game->world().map();

    const double mapW = std::max(1, map.widthTiles());
    const double mapH = std::max(1, map.heightTiles());

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

QPointF MiniMapWidget::widgetToWorld(const QPoint& pos) const
{
    if (!m_game || !m_game->world().hasMap()) {
        return QPointF();
    }

    const WorldMap& map = m_game->world().map();
    const QSize worldSize = map.worldSizePx();
    const QRectF target = mapTargetRect();

    if (target.width() <= 0.0 || target.height() <= 0.0) {
        return QPointF();
    }

    const double nx = std::clamp((pos.x() - target.left()) / target.width(), 0.0, 1.0);
    const double ny = std::clamp((pos.y() - target.top()) / target.height(), 0.0, 1.0);

    return QPointF(
        nx * worldSize.width(),
        ny * worldSize.height()
        );
}

void MiniMapWidget::centerCameraAtWidgetPos(const QPoint& pos)
{
    if (!m_game) {
        return;
    }

    m_game->setCameraCenterWorld(widgetToWorld(pos));
}
