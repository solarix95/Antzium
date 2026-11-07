#include "explosioneffect.h"

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

ExplosionEffect::ExplosionEffect(
    const QPointF& worldPos,
    double radiusPx,
    const QColor& groundColor,
    QObject* parent
    )
    : AbstractEffect(parent)
    , m_origin(worldPos)
    , m_radius(radiusPx)
    , m_groundColor(groundColor)
{
    m_smokeColor = groundColor.darker(185);
    m_ringColor = groundColor.lighter(135);
    const int particleCount =
        70 + static_cast<int>(QRandomGenerator::global()->bounded(45));

    m_particles.reserve(particleCount);

    for (int i = 0; i < particleCount; ++i) {
        const double angle =
            QRandomGenerator::global()->generateDouble() * Pi * 2.0;

        const double speed =
            m_radius * (0.65 + QRandomGenerator::global()->generateDouble() * 2.25);

        Particle particle;
        particle.velocity = QPointF(
            std::cos(angle) * speed,
            std::sin(angle) * speed
            );

        particle.size =
            1.4 + QRandomGenerator::global()->generateDouble() * 5.2;

        particle.lifeTime =
            0.35 + QRandomGenerator::global()->generateDouble() * 0.58;

        switch (static_cast<int>(QRandomGenerator::global()->bounded(6))) {
        case 0:
            particle.color = QColor(255, 242, 150);
            break;

        case 1:
            particle.color = QColor(255, 156, 42);
            break;

        case 2:
            particle.color = QColor(210, 70, 34);
            break;

        default:
            particle.color = shiftedColor(
                m_groundColor,
                static_cast<int>(QRandomGenerator::global()->bounded(61)) - 30,
                static_cast<int>(QRandomGenerator::global()->bounded(61)) - 30,
                static_cast<int>(QRandomGenerator::global()->bounded(61)) - 30
                );
            break;
        }

        m_particles.push_back(particle);
    }
}

void ExplosionEffect::drawEffect(QPainter& painter, const QRectF& cameraWorldRect)
{
    const double t = timeElapsed();

    if (t >= m_duration) {
        deleteLater();
        return;
    }

    const QRectF effectBounds(
        m_origin.x() - m_radius - 130.0,
        m_origin.y() - m_radius - 130.0,
        m_radius * 2.0 + 260.0,
        m_radius * 2.0 + 260.0
        );

    if (!cameraWorldRect.intersects(effectBounds)) {
        return;
    }

    const double age = std::clamp(t / m_duration, 0.0, 1.0);
    const double fade = 1.0 - age;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(Qt::NoPen);

    const double flashRadius = m_radius * (0.25 + age * 0.42);
    painter.setBrush(withAlpha(QColor(255, 248, 170), static_cast<int>(180.0 * fade)));
    painter.drawEllipse(m_origin, flashRadius, flashRadius);

    const double smokeRadius = m_radius * (0.55 + age * 0.75);
    painter.setBrush(withAlpha(m_smokeColor, static_cast<int>(115.0 * fade)));
    painter.drawEllipse(m_origin, smokeRadius, smokeRadius * 0.72);

    painter.setBrush(withAlpha(QColor(250, 122, 35), static_cast<int>(155.0 * fade)));
    painter.drawEllipse(m_origin, m_radius * (0.28 + age * 0.28), m_radius * (0.22 + age * 0.22));

    painter.setPen(QPen(withAlpha(m_ringColor, static_cast<int>(220.0 * fade)), 2.3));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(m_origin, m_radius * (0.90 + age * 0.18), m_radius * (0.90 + age * 0.18));

    painter.setPen(Qt::NoPen);

    for (const Particle& particle : m_particles) {
        const double particleAge = std::clamp(t / particle.lifeTime, 0.0, 1.0);

        if (particleAge >= 1.0) {
            continue;
        }

        const double particleFade = 1.0 - particleAge;
        const double travel = 1.0 - std::pow(1.0 - particleAge, 2.0);

        QPointF pos(
            m_origin.x() + particle.velocity.x() * particle.lifeTime * travel,
            m_origin.y() + particle.velocity.y() * particle.lifeTime * travel
            );

        pos.ry() += 26.0 * particleAge * particleAge;

        QColor color = particle.color;
        color.setAlpha(static_cast<int>(225.0 * particleFade));

        const double size = particle.size * (1.0 - particleAge * 0.35);

        painter.setBrush(color);
        painter.drawRect(QRectF(pos.x() - size * 0.5, pos.y() - size * 0.5, size, size));
    }

    painter.restore();
}
