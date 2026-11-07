
#include <algorithm>
#include <cmath>

#include <QColor>
#include <QPainter>

#include "pheromonefield.h"
#include "worldmap.h"


void PheromoneField::reset(int widthTiles, int heightTiles, int tileSize)
{
    m_widthTiles = widthTiles;
    m_heightTiles = heightTiles;
    m_tileSize = tileSize;

    m_cells.clear();
    m_cells.resize(m_widthTiles * m_heightTiles);

    m_buffer.clear();
    m_buffer.resize(m_widthTiles * m_heightTiles);
}

void PheromoneField::clear()
{
    for (PheromoneCell& cell : m_cells) {
        cell.food = 0.0;
        cell.home = 0.0;
    }

    for (PheromoneCell& cell : m_buffer) {
        cell.food = 0.0;
        cell.home = 0.0;
    }
}

bool PheromoneField::isValid() const
{
    return m_widthTiles > 0 && m_heightTiles > 0 && !m_cells.isEmpty();
}

void PheromoneField::deposit(PheromoneKind kind, const QPointF& worldPos, double amount)
{
    if (!isValid()) {
        return;
    }

    const int tx = static_cast<int>(std::floor(worldPos.x() / m_tileSize));
    const int ty = static_cast<int>(std::floor(worldPos.y() / m_tileSize));

    if (!isInside(tx, ty)) {
        return;
    }

    PheromoneCell& cell = m_cells[index(tx, ty)];

    const double oldValue = valueOf(cell, kind);
    const double newValue = std::clamp(oldValue + amount, 0.0, 255.0);

    setValue(cell, kind, newValue);
}

double PheromoneField::sample(PheromoneKind kind, const QPointF& worldPos) const
{
    if (!isValid()) {
        return 0.0;
    }

    const int tx = static_cast<int>(std::floor(worldPos.x() / m_tileSize));
    const int ty = static_cast<int>(std::floor(worldPos.y() / m_tileSize));

    if (!isInside(tx, ty)) {
        return 0.0;
    }

    return valueOf(m_cells[index(tx, ty)], kind);
}

void PheromoneField::depositCircle(
    PheromoneKind kind,
    const QPointF& worldPos,
    double radiusPx,
    double amount,
    const WorldMap& map
    )
{
    if (!isValid()) {
        return;
    }

    const double radiusSq = radiusPx * radiusPx;

    const int x0 = std::max(0, static_cast<int>(std::floor((worldPos.x() - radiusPx) / m_tileSize)));
    const int y0 = std::max(0, static_cast<int>(std::floor((worldPos.y() - radiusPx) / m_tileSize)));

    const int x1 = std::min(m_widthTiles - 1, static_cast<int>(std::ceil((worldPos.x() + radiusPx) / m_tileSize)));
    const int y1 = std::min(m_heightTiles - 1, static_cast<int>(std::ceil((worldPos.y() + radiusPx) / m_tileSize)));

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            if (!map.isWalkableTile(x, y)) {
                continue;
            }

            const QPointF center(
                x * m_tileSize + m_tileSize * 0.5,
                y * m_tileSize + m_tileSize * 0.5
                );

            const double dx = center.x() - worldPos.x();
            const double dy = center.y() - worldPos.y();
            const double d2 = dx * dx + dy * dy;

            if (d2 > radiusSq) {
                continue;
            }

            const double d = std::sqrt(d2);
            const double falloff = std::clamp(1.0 - d / radiusPx, 0.0, 1.0);

            PheromoneCell& cell = m_cells[index(x, y)];

            const double oldValue = valueOf(cell, kind);
            const double newValue = std::clamp(oldValue + amount * falloff, 0.0, 255.0);

            setValue(cell, kind, newValue);
        }
    }
}

void PheromoneField::clearCircle(const QPointF& worldPos, double radiusPx)
{
    if (!isValid()) {
        return;
    }

    const double radiusSq = radiusPx * radiusPx;

    const int x0 = std::max(0, static_cast<int>(std::floor((worldPos.x() - radiusPx) / m_tileSize)));
    const int y0 = std::max(0, static_cast<int>(std::floor((worldPos.y() - radiusPx) / m_tileSize)));

    const int x1 = std::min(m_widthTiles - 1, static_cast<int>(std::ceil((worldPos.x() + radiusPx) / m_tileSize)));
    const int y1 = std::min(m_heightTiles - 1, static_cast<int>(std::ceil((worldPos.y() + radiusPx) / m_tileSize)));

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const QPointF center(
                x * m_tileSize + m_tileSize * 0.5,
                y * m_tileSize + m_tileSize * 0.5
                );

            const double dx = center.x() - worldPos.x();
            const double dy = center.y() - worldPos.y();

            if (dx * dx + dy * dy <= radiusSq) {
                PheromoneCell& cell = m_cells[index(x, y)];
                cell.food = 0.0;
                cell.home = 0.0;
            }
        }
    }
}

void PheromoneField::evaporateAndDiffuse(double dt, const WorldMap& map)
{
    if (!isValid()) {
        return;
    }

    const double evaporation = std::pow(0.90, dt);
    const double diffusion = std::clamp(0.35 * dt, 0.0, 0.25);

    for (int y = 0; y < m_heightTiles; ++y) {
        for (int x = 0; x < m_widthTiles; ++x) {
            const int i = index(x, y);

            if (!map.isWalkableTile(x, y)) {
                m_buffer[i].food = 0.0;
                m_buffer[i].home = 0.0;
                continue;
            }

            double foodSum = m_cells[i].food;
            double homeSum = m_cells[i].home;
            double count = 1.0;

            const int dx[4] = { 1, -1, 0, 0 };
            const int dy[4] = { 0, 0, 1, -1 };

            for (int n = 0; n < 4; ++n) {
                const int nx = x + dx[n];
                const int ny = y + dy[n];

                if (!isInside(nx, ny)) {
                    continue;
                }

                if (!map.isWalkableTile(nx, ny)) {
                    continue;
                }

                const PheromoneCell& other = m_cells[index(nx, ny)];

                foodSum += other.food;
                homeSum += other.home;
                count += 1.0;
            }

            const double foodAverage = foodSum / count;
            const double homeAverage = homeSum / count;

            m_buffer[i].food = ((1.0 - diffusion) * m_cells[i].food + diffusion * foodAverage) * evaporation;
            m_buffer[i].home = ((1.0 - diffusion) * m_cells[i].home + diffusion * homeAverage) * evaporation;

            if (m_buffer[i].food < 0.01) {
                m_buffer[i].food = 0.0;
            }

            if (m_buffer[i].home < 0.01) {
                m_buffer[i].home = 0.0;
            }
        }
    }

    m_cells.swap(m_buffer);
}

void PheromoneField::draw(QPainter& painter, const QRectF& cameraWorldRect) const
{
    if (!isValid()) {
        return;
    }

    const int x0 = std::max(0, static_cast<int>(std::floor(cameraWorldRect.left() / m_tileSize)));
    const int y0 = std::max(0, static_cast<int>(std::floor(cameraWorldRect.top() / m_tileSize)));

    const int x1 = std::min(m_widthTiles - 1, static_cast<int>(std::ceil(cameraWorldRect.right() / m_tileSize)));
    const int y1 = std::min(m_heightTiles - 1, static_cast<int>(std::ceil(cameraWorldRect.bottom() / m_tileSize)));

    painter.setPen(Qt::NoPen);

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const PheromoneCell& cell = m_cells[index(x, y)];

            const int foodAlpha = std::clamp(static_cast<int>(cell.food * 0.55), 0, 120);
            const int homeAlpha = std::clamp(static_cast<int>(cell.home * 0.25), 0, 70);

            const QRectF r(
                x * m_tileSize,
                y * m_tileSize,
                m_tileSize,
                m_tileSize
                );

            if (homeAlpha > 3) {
                painter.fillRect(r, QColor(80, 150, 255, homeAlpha));
            }

            if (foodAlpha > 3) {
                painter.fillRect(r, QColor(255, 190, 40, foodAlpha));
            }
        }
    }
}

bool PheromoneField::isInside(int tx, int ty) const
{
    return tx >= 0 && ty >= 0 && tx < m_widthTiles && ty < m_heightTiles;
}

int PheromoneField::index(int tx, int ty) const
{
    return ty * m_widthTiles + tx;
}

double PheromoneField::valueOf(const PheromoneCell& cell, PheromoneKind kind) const
{
    switch (kind) {
    case PheromoneKind::Food:
        return cell.food;

    case PheromoneKind::Home:
        return cell.home;
    }

    return 0.0;
}

void PheromoneField::setValue(PheromoneCell& cell, PheromoneKind kind, double value)
{
    switch (kind) {
    case PheromoneKind::Food:
        cell.food = value;
        break;

    case PheromoneKind::Home:
        cell.home = value;
        break;
    }
}
