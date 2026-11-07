#ifndef BURNEFFECT_H
#define BURNEFFECT_H

#include "abstracteffect.h"

#include <QColor>
#include <QPointF>
#include <QVector>

class BurnEffect : public AbstractEffect
{
public:
    explicit BurnEffect(
        const QPointF& worldPos,
        double radiusPx,
        QObject* parent = nullptr
        );

    void drawEffect(QPainter& painter, const QRectF& cameraWorldRect) override;

private:
    struct Spark
    {
        QPointF velocity;
        QColor color;
        double size = 2.0;
        double lifeTime = 0.4;
    };

private:
    QPointF m_origin;
    double m_radius = 48.0;
    double m_duration = 0.55;

    QVector<Spark> m_sparks;
};

#endif // BURNEFFECT_H
