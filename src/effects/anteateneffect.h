#ifndef ANTEATENEFFECT_H
#define ANTEATENEFFECT_H

#include "abstracteffect.h"

#include <QColor>
#include <QPointF>
#include <QVector>

class AntEatenEffect : public AbstractEffect
{
public:
    explicit AntEatenEffect(
        const QPointF& worldPos,
        const QColor& antColor,
        QObject* parent = nullptr
        );

    void drawEffect(QPainter& painter, const QRectF& cameraWorldRect) override;

private:
    struct Particle
    {
        QPointF velocity;
        QColor color;
        double size = 2.0;
        double lifeTime = 0.7;
    };

private:
    QPointF m_origin;
    QVector<Particle> m_particles;
    double m_duration = 0.85;
};

#endif // ANTEATENEFFECT_H
