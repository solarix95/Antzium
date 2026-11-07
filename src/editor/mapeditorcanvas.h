#ifndef MAPEDITORCANVAS_H
#define MAPEDITORCANVAS_H

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QRectF>
#include <QWidget>

class MapEditorCanvas : public QWidget
{
    Q_OBJECT

public:
    enum class Tool
    {
        Ground,
        Grass,
        Rock,
        Water,
        ShallowWater,
        Food,
        Spider,
        Flower,
        SpiderHole,
        ColonyRed,
        ColonyMagenta,
        ColonyCoral,
        QueenRed,
        QueenMagenta,
        QueenCoral
    };

    explicit MapEditorCanvas(QWidget* parent = nullptr);

    const QImage& image() const;
    void setImage(const QImage& image);

    void createBlankMap(const QSize& size);
    void randomize();

    void setTool(Tool tool);

signals:
    void imageChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QRectF mapTargetRect() const;
    QPoint widgetToImagePixel(const QPoint& pos) const;

    void paintAtWidgetPos(const QPoint& pos);
    void paintPixel(int x, int y);
    void paintTerrainBrush(int cx, int cy, const QColor& color);
    void clearExistingMarkerColor(const QColor& color);

    QColor colorForTool(Tool tool) const;
    bool isTerrainTool(Tool tool) const;

    void drawOrganicBlob(
        QImage& img,
        const QPoint& center,
        int rx,
        int ry,
        const QColor& color,
        int passes = 1
        );

    void drawDisc(
        QImage& img,
        const QPoint& center,
        int radius,
        const QColor& color,
        int chancePercent = 100
        );

    void drawLake(QImage& img, const QPoint& center, int rx, int ry);
    void drawMeanderingRiver(QImage& img);
    void drawRockRidge(QImage& img, const QPoint& start, const QPoint& end, int width);

    bool isGroundLikeColor(const QColor& color) const;

    QPoint randomPointOnGroundLike(
        const QVector<QPoint>& avoid,
        int minDistanceTiles
        ) const;

    QPoint randomPointInRectOnGroundLike(
        const QRect& area,
        const QVector<QPoint>& avoid,
        int minDistanceTiles
        ) const;

    QPoint randomPointOnGroundLike() const;

private:
    QImage m_image;
    Tool m_tool = Tool::Ground;

    int m_terrainBrushRadius = 2;
};

#endif // MAPEDITORCANVAS_H
