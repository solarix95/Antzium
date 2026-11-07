#ifndef EXPLOSIONEFFECT_H
#define EXPLOSIONEFFECT_H

#include "abstracteffect.h"

#include <QColor>
#include <QPointF>
#include <QVector>

class ExplosionEffect : public AbstractEffect
{
public:
    explicit ExplosionEffect(
        const QPointF& worldPos,
        double radiusPx,
        const QColor& groundColor,
        QObject* parent = nullptr
        );

    void drawEffect(QPainter& painter, const QRectF& cameraWorldRect) override;

private:
    struct Particle
    {
        QPointF velocity;
        QColor color;
        double size = 2.0;
        double lifeTime = 0.8;
    };

private:
    QPointF m_origin;
    double m_radius = 70.0;
    double m_duration = 0.95;

    QColor m_groundColor;
    QColor m_smokeColor;
    QColor m_ringColor;

    QVector<Particle> m_particles;
};

#endif // EXPLOSIONEFFECT_H
