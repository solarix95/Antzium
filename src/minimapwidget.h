#pragma once

#include <QImage>
#include <QPointer>
#include <QRectF>
#include <QTimer>
#include <QWidget>

class GameWidget;

class MiniMapWidget : public QWidget
{
public:
    explicit MiniMapWidget(GameWidget* game, QWidget* parent = nullptr);

    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void rebuildMapImage();
    QRectF mapTargetRect() const;
    QPointF widgetToWorld(const QPoint& pos) const;
    void centerCameraAtWidgetPos(const QPoint& pos);

private:
    QPointer<GameWidget> m_game;
    QImage m_mapImage;
    bool m_dirty = true;
    quint64 m_terrainRevision = 0;
    QTimer m_terrainRevisionTimer;
};
