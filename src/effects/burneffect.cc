#include "burneffect.h"

#include <QPen>
#include <QRandomGenerator>

#include <algorithm>
#include <cmath>

namespace
{
constexpr double Pi = 3.14159265358979323846;

QColor withAlpha(QColor color, int alpha)
{
    color.setAlpha(std::clamp(alpha, 0, 255));
    return color;
}
}

BurnEffect::BurnEffect(
    const QPointF& worldPos,
    double radiusPx,
    QObject* parent
    )
    : AbstractEffect(parent)
    , m_origin(worldPos)
    , m_radius(radiusPx)
{
    const int sparkCount =
        28 + static_cast<int>(QRandomGenerator::global()->bounded(18));

    m_sparks.reserve(sparkCount);

    for (int i = 0; i < sparkCount; ++i) {
        const double angle =
            QRandomGenerator::global()->generateDouble() * Pi * 2.0;

        const double speed =
            m_radius * (0.7 + QRandomGenerator::global()->generateDouble() * 1.35);

        Spark spark;

        spark.velocity = QPointF(
            std::cos(angle) * speed,
            std::sin(angle) * speed
            );

        spark.size =
            1.0 + QRandomGenerator::global()->generateDouble() * 2.8;

        spark.lifeTime =
            0.22 + QRandomGenerator::global()->generateDouble() * 0.36;

        const int palette =
            static_cast<int>(QRandomGenerator::global()->bounded(4));

        switch (palette) {
        case 0:
            spark.color = QColor(255, 235, 120);
            break;

        case 1:
            spark.color = QColor(255, 165, 55);
            break;

        case 2:
            spark.color = QColor(255, 95, 35);
            break;

        default:
            spark.color = QColor(230, 230, 210);
            break;
        }

        m_sparks.push_back(spark);
    }
}

void BurnEffect::drawEffect(QPainter& painter, const QRectF& cameraWorldRect)
{
    const double t = timeElapsed();

    if (t >= m_duration) {
        deleteLater();
        return;
    }

    const QRectF effectBounds(
        m_origin.x() - m_radius - 80.0,
        m_origin.y() - m_radius - 80.0,
        m_radius * 2.0 + 160.0,
        m_radius * 2.0 + 160.0
        );

    if (!cameraWorldRect.intersects(effectBounds)) {
        return;
    }

    const double age = std::clamp(t / m_duration, 0.0, 1.0);
    const double fade = 1.0 - age;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Kurzer heller Flash im Zentrum.
    {
        const double flashRadius = m_radius * (0.20 + age * 0.30);
        const int alpha = static_cast<int>(120.0 * fade);

        painter.setPen(Qt::NoPen);
        painter.setBrush(withAlpha(QColor(255, 240, 150), alpha));

        painter.drawEllipse(
            m_origin,
            flashRadius,
            flashRadius
            );
    }

    // Hauptkreis: zeigt den betroffenen Burn-Radius.
    {
        const double ringRadius = m_radius * (0.92 + age * 0.18);
        const int alpha = static_cast<int>(220.0 * fade);

        QPen pen(withAlpha(QColor(255, 210, 70), alpha), 2.0);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        painter.drawEllipse(
            m_origin,
            ringRadius,
            ringRadius
            );
    }

    // Zweiter dünner, rötlicher Außenring.
    {
        const double ringRadius = m_radius * (0.65 + age * 0.55);
        const int alpha = static_cast<int>(120.0 * fade);

        QPen pen(withAlpha(QColor(255, 80, 35), alpha), 1.2);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        painter.drawEllipse(
            m_origin,
            ringRadius,
            ringRadius
            );
    }

    // Kleine Funken / Pixel, die nach außen fliegen.
    painter.setPen(Qt::NoPen);

    for (const Spark& spark : m_sparks) {
        const double sparkAge = std::clamp(t / spark.lifeTime, 0.0, 1.0);

        if (sparkAge >= 1.0) {
            continue;
        }

        const double sparkFade = 1.0 - sparkAge;

        // Leicht abbremsende Bewegung.
        const double travel =
            1.0 - std::pow(1.0 - sparkAge, 2.0);

        QPointF pos(
            m_origin.x() + spark.velocity.x() * spark.lifeTime * travel,
            m_origin.y() + spark.velocity.y() * spark.lifeTime * travel
            );

        // Hitzefunken steigen ganz leicht nach oben.
        pos.ry() -= 12.0 * sparkAge * sparkAge;

        QColor color = spark.color;
        color.setAlpha(static_cast<int>(230.0 * sparkFade));

        const double size =
            spark.size * (1.0 - sparkAge * 0.45);

        painter.setBrush(color);
        painter.drawRect(
            QRectF(
                pos.x() - size * 0.5,
                pos.y() - size * 0.5,
                size,
                size
                )
            );
    }

    painter.restore();
}
