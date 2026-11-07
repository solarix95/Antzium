#include "anteateneffect.h"

#include <QRandomGenerator>

#include <algorithm>
#include <cmath>

namespace
{
constexpr double Pi = 3.14159265358979323846;

QColor shiftedColor(const QColor& base, int dr, int dg, int db)
{
    return QColor(
        std::clamp(base.red() + dr, 0, 255),
        std::clamp(base.green() + dg, 0, 255),
        std::clamp(base.blue() + db, 0, 255),
        base.alpha()
        );
}
}

AntEatenEffect::AntEatenEffect(
    const QPointF& worldPos,
    const QColor& antColor,
    QObject* parent
    )
    : AbstractEffect(parent)
    , m_origin(worldPos)
{
    const int particleCount = 18 + static_cast<int>(QRandomGenerator::global()->bounded(10));

    m_particles.reserve(particleCount);

    for (int i = 0; i < particleCount; ++i) {
        const double angle =
            QRandomGenerator::global()->generateDouble() * Pi * 2.0;

        const double speed =
            22.0 + QRandomGenerator::global()->generateDouble() * 58.0;

        Particle p;
        p.velocity = QPointF(
            std::cos(angle) * speed,
            std::sin(angle) * speed
            );

        p.size = 1.2 + QRandomGenerator::global()->generateDouble() * 2.2;
        p.lifeTime = 0.35 + QRandomGenerator::global()->generateDouble() * 0.55;

        // Hauptsächlich Ant-Farbe, leicht variiert.
        p.color = shiftedColor(
            antColor,
            static_cast<int>(QRandomGenerator::global()->bounded(51)) - 25,
            static_cast<int>(QRandomGenerator::global()->bounded(51)) - 25,
            static_cast<int>(QRandomGenerator::global()->bounded(51)) - 25
            );

        m_particles.push_back(p);
    }

    // Ein paar helle "Sabber"/Biss-Pixel.
    for (int i = 0; i < 5; ++i) {
        const double angle =
            QRandomGenerator::global()->generateDouble() * Pi * 2.0;

        const double speed =
            12.0 + QRandomGenerator::global()->generateDouble() * 32.0;

        Particle p;
        p.velocity = QPointF(
            std::cos(angle) * speed,
            std::sin(angle) * speed
            );

        p.size = 1.0 + QRandomGenerator::global()->generateDouble() * 1.4;
        p.lifeTime = 0.25 + QRandomGenerator::global()->generateDouble() * 0.35;
        p.color = QColor(245, 230, 150);

        m_particles.push_back(p);
    }
}

void AntEatenEffect::drawEffect(QPainter& painter, const QRectF& cameraWorldRect)
{
    const double t = timeElapsed();

    if (t >= m_duration) {
        deleteLater();
        return;
    }

    const QRectF effectBounds(
        m_origin.x() - 90.0,
        m_origin.y() - 90.0,
        180.0,
        180.0
        );

    if (!cameraWorldRect.intersects(effectBounds)) {
        return;
    }

    painter.save();
    painter.setPen(Qt::NoPen);

    for (const Particle& p : m_particles) {
        const double normalizedAge = std::clamp(t / p.lifeTime, 0.0, 1.0);

        if (normalizedAge >= 1.0) {
            continue;
        }

        // Kleine Bremsung: am Anfang schnell, dann auslaufend.
        const double travelFactor = 1.0 - std::pow(1.0 - normalizedAge, 2.0);

        QPointF pos(
            m_origin.x() + p.velocity.x() * p.lifeTime * travelFactor,
            m_origin.y() + p.velocity.y() * p.lifeTime * travelFactor
            );

        // Minimale Gravitation nach unten für etwas organischeren Look.
        pos.ry() += 18.0 * normalizedAge * normalizedAge;

        QColor c = p.color;
        c.setAlpha(static_cast<int>(220.0 * (1.0 - normalizedAge)));

        const double size = p.size * (1.0 - normalizedAge * 0.35);

        painter.setBrush(c);
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
