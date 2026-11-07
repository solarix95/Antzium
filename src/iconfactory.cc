#include "iconfactory.h"

#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QPixmap>
#include <QPolygonF>

#include <algorithm>
#include <cmath>

namespace
{
QRectF unitRect(const QSize& size, double margin = 3.0)
{
    return QRectF(
        margin,
        margin,
        size.width() - margin * 2.0,
        size.height() - margin * 2.0
        );
}

QPointF p(const QSize& size, double x, double y)
{
    return QPointF(size.width() * x, size.height() * y);
}

QColor withAlpha(QColor color, int alpha)
{
    color.setAlpha(alpha);
    return color;
}

void drawGround(QPainter& painter, const QSize& size)
{
    QRectF r = unitRect(size, 4);

    painter.setPen(QPen(QColor(135, 105, 65), 1.4));
    painter.setBrush(QColor(225, 210, 160));
    painter.drawRoundedRect(r, 4, 4);

    painter.setPen(QPen(QColor(165, 135, 85), 1.2));
    painter.drawPoint(p(size, 0.35, 0.38));
    painter.drawPoint(p(size, 0.58, 0.55));
    painter.drawPoint(p(size, 0.43, 0.68));
    painter.drawPoint(p(size, 0.70, 0.35));
}

void drawGrass(QPainter& painter, const QSize& size)
{
    QRectF r = unitRect(size, 4);

    painter.setPen(QPen(QColor(35, 90, 38), 1.4));
    painter.setBrush(QColor(72, 145, 62));
    painter.drawRoundedRect(r, 4, 4);

    painter.setPen(QPen(QColor(145, 210, 95), 1.5));

    for (int i = 0; i < 7; ++i) {
        const double x = 0.22 + i * 0.09;
        painter.drawLine(
            p(size, x, 0.72),
            p(size, x + ((i % 2 == 0) ? 0.03 : -0.025), 0.32)
            );
    }
}

void drawRock(QPainter& painter, const QSize& size)
{
    QPolygonF poly;
    poly << p(size, 0.22, 0.68)
         << p(size, 0.33, 0.34)
         << p(size, 0.55, 0.20)
         << p(size, 0.78, 0.42)
         << p(size, 0.72, 0.72)
         << p(size, 0.43, 0.80);

    painter.setPen(QPen(QColor(45, 45, 45), 1.5));
    painter.setBrush(QColor(110, 110, 108));
    painter.drawPolygon(poly);

    painter.setPen(QPen(QColor(155, 155, 150), 1.2));
    painter.drawLine(p(size, 0.43, 0.35), p(size, 0.60, 0.28));
    painter.drawLine(p(size, 0.35, 0.58), p(size, 0.58, 0.66));

    painter.setPen(QPen(QColor(60, 60, 60), 1.2));
    painter.drawLine(p(size, 0.54, 0.38), p(size, 0.67, 0.52));
}

void drawWater(QPainter& painter, const QSize& size, bool shallow)
{
    QRectF r = unitRect(size, 4);

    QColor base = shallow ? QColor(102, 204, 255) : QColor(30, 78, 165);
    QColor edge = shallow ? QColor(50, 145, 205) : QColor(15, 45, 120);
    QColor wave = shallow ? QColor(220, 250, 255) : QColor(125, 175, 235);

    painter.setPen(QPen(edge, 1.4));
    painter.setBrush(base);
    painter.drawRoundedRect(r, 4, 4);

    painter.setPen(QPen(wave, 1.7));

    painter.drawArc(
        QRectF(size.width() * 0.18, size.height() * 0.35, size.width() * 0.26, size.height() * 0.18),
        0,
        180 * 16
        );

    painter.drawArc(
        QRectF(size.width() * 0.42, size.height() * 0.35, size.width() * 0.26, size.height() * 0.18),
        0,
        180 * 16
        );

    painter.drawArc(
        QRectF(size.width() * 0.30, size.height() * 0.56, size.width() * 0.28, size.height() * 0.18),
        0,
        180 * 16
        );

    if (shallow) {
        painter.setPen(QPen(QColor(220, 205, 135, 150), 1.5));
        painter.drawPoint(p(size, 0.30, 0.72));
        painter.drawPoint(p(size, 0.63, 0.25));
    }
}

void drawFood(QPainter& painter, const QSize& size)
{
    painter.setPen(QPen(QColor(130, 80, 20), 1.2));
    painter.setBrush(QColor(235, 190, 40));
    painter.drawEllipse(QRectF(size.width() * 0.25, size.height() * 0.34, size.width() * 0.50, size.height() * 0.46));

    painter.setPen(QPen(QColor(70, 105, 35), 1.2));
    painter.setBrush(QColor(80, 155, 55));
    painter.drawEllipse(QRectF(size.width() * 0.55, size.height() * 0.20, size.width() * 0.18, size.height() * 0.12));

    painter.setPen(QPen(QColor(95, 55, 25), 1.8));
    painter.drawLine(p(size, 0.50, 0.34), p(size, 0.56, 0.20));

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 230, 95, 160));
    painter.drawEllipse(QRectF(size.width() * 0.35, size.height() * 0.43, size.width() * 0.13, size.height() * 0.10));
}

void drawPheromone(QPainter& painter, const QSize& size)
{
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(QPen(QColor(255, 190, 40), 2.0, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);

    QRectF r(
        size.width() * 0.25,
        size.height() * 0.25,
        size.width() * 0.50,
        size.height() * 0.50
        );

    painter.drawArc(r, 30 * 16, 285 * 16);

    QRectF r2(
        size.width() * 0.36,
        size.height() * 0.36,
        size.width() * 0.28,
        size.height() * 0.28
        );

    painter.drawArc(r2, 210 * 16, 260 * 16);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 210, 70));

    painter.drawEllipse(p(size, 0.72, 0.32), 2.2, 2.2);
    painter.drawEllipse(p(size, 0.27, 0.66), 1.8, 1.8);
    painter.drawEllipse(p(size, 0.58, 0.72), 1.5, 1.5);
}

void drawDynamite(QPainter& painter, const QSize& size)
{
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 55));
    painter.drawEllipse(QRectF(size.width() * 0.22, size.height() * 0.72, size.width() * 0.56, size.height() * 0.10));

    QRectF body(size.width() * 0.23, size.height() * 0.34, size.width() * 0.54, size.height() * 0.30);

    painter.setPen(QPen(QColor(90, 20, 18), 1.4));
    painter.setBrush(QColor(205, 38, 35));
    painter.drawRoundedRect(body, 5, 5);

    painter.setBrush(QColor(245, 80, 70));
    painter.drawRoundedRect(QRectF(size.width() * 0.27, size.height() * 0.38, size.width() * 0.18, size.height() * 0.08), 3, 3);

    painter.setPen(QPen(QColor(245, 220, 90), 2.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(p(size, 0.50, 0.33), p(size, 0.50, 0.20));
    painter.drawLine(p(size, 0.50, 0.20), p(size, 0.63, 0.14));

    const QFont oldFont = painter.font();
    QFont font = oldFont;
    font.setBold(true);
    font.setPointSizeF(std::max(6.0, size.height() * 0.20));
    painter.setFont(font);

    painter.setPen(QColor(255, 245, 210));
    painter.drawText(body, Qt::AlignCenter, QStringLiteral("TNT"));

    painter.setFont(oldFont);
}

void drawMagnifier(QPainter& painter, const QSize& size)
{
    painter.setPen(QPen(QColor(245, 235, 150), 2.2));
    painter.setBrush(QColor(255, 220, 70, 45));

    painter.drawEllipse(QRectF(size.width() * 0.18, size.height() * 0.15, size.width() * 0.48, size.height() * 0.48));

    painter.setPen(QPen(QColor(230, 210, 90), 3.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(p(size, 0.57, 0.58), p(size, 0.82, 0.82));

    painter.setPen(QPen(QColor(255, 90, 35), 1.4));
    painter.drawEllipse(QRectF(size.width() * 0.31, size.height() * 0.28, size.width() * 0.20, size.height() * 0.20));
}

void drawPointer(QPainter& painter, const QSize& size)
{
    QPolygonF cursor;
    cursor << p(size, 0.25, 0.16)
           << p(size, 0.25, 0.78)
           << p(size, 0.43, 0.62)
           << p(size, 0.54, 0.84)
           << p(size, 0.66, 0.78)
           << p(size, 0.55, 0.57)
           << p(size, 0.78, 0.57);

    painter.setPen(QPen(QColor(20, 25, 30), 2.0));
    painter.setBrush(QColor(235, 240, 245));
    painter.drawPolygon(cursor);
}

void drawSpeedTriangle(QPainter& painter, const QSize& size, int count)
{
    painter.setPen(QPen(QColor(25, 80, 35), 1.4));
    painter.setBrush(QColor(90, 210, 90));

    if (count <= 1) {
        QPolygonF play;
        play << p(size, 0.34, 0.24)
             << p(size, 0.34, 0.76)
             << p(size, 0.76, 0.50);

        painter.drawPolygon(play);
        return;
    }

    QPolygonF a;
    a << p(size, 0.22, 0.24)
      << p(size, 0.22, 0.76)
      << p(size, 0.52, 0.50);

    QPolygonF b;
    b << p(size, 0.48, 0.24)
      << p(size, 0.48, 0.76)
      << p(size, 0.78, 0.50);

    painter.drawPolygon(a);
    painter.drawPolygon(b);
}

void drawSpider(QPainter& painter, const QSize& size)
{
    QPointF center = p(size, 0.50, 0.54);

    painter.setPen(QPen(QColor(20, 18, 24), 1.5));

    for (int i = 0; i < 4; ++i) {
        const double y = 0.34 + i * 0.12;

        painter.drawLine(p(size, 0.44, y), p(size, 0.16, y - 0.08 + i * 0.04));
        painter.drawLine(p(size, 0.56, y), p(size, 0.84, y - 0.08 + i * 0.04));
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(35, 30, 42));
    painter.drawEllipse(QRectF(center.x() - size.width() * 0.18, center.y() - size.height() * 0.16, size.width() * 0.36, size.height() * 0.30));

    painter.setBrush(QColor(24, 22, 30));
    painter.drawEllipse(QRectF(size.width() * 0.39, size.height() * 0.22, size.width() * 0.22, size.height() * 0.20));

    painter.setBrush(QColor(180, 40, 40));
    painter.drawEllipse(QRectF(size.width() * 0.44, size.height() * 0.30, size.width() * 0.04, size.height() * 0.04));
    painter.drawEllipse(QRectF(size.width() * 0.52, size.height() * 0.30, size.width() * 0.04, size.height() * 0.04));
}

void drawFlower(QPainter& painter, const QSize& size)
{
    const QPointF center = p(size, 0.50, 0.48);

    painter.setPen(QPen(QColor(45, 105, 45), 1.8));
    painter.drawLine(p(size, 0.50, 0.82), p(size, 0.50, 0.55));

    painter.setPen(Qt::NoPen);

    const QColor petal = QColor(200, 80, 230);
    const QColor petalDark = QColor(130, 45, 165);

    const QPointF offsets[] = {
        QPointF(0.0, -7.0),
        QPointF(6.0, -2.0),
        QPointF(4.0, 5.0),
        QPointF(-4.0, 5.0),
        QPointF(-6.0, -2.0)
    };

    painter.setBrush(petalDark);

    for (const QPointF& o : offsets) {
        painter.drawEllipse(center + o, 5.0, 5.0);
    }

    painter.setBrush(petal);

    for (const QPointF& o : offsets) {
        painter.drawEllipse(center + o * 0.9, 3.8, 3.8);
    }

    painter.setBrush(QColor(240, 210, 60));
    painter.drawEllipse(center, 3.0, 3.0);
}

void drawColony(QPainter& painter, const QSize& size, QColor color)
{
    painter.setPen(QPen(QColor(40, 30, 30), 1.5));
    painter.setBrush(color);
    painter.drawEllipse(QRectF(size.width() * 0.22, size.height() * 0.20, size.width() * 0.56, size.height() * 0.56));

    painter.setBrush(withAlpha(QColor(255, 255, 255), 85));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QRectF(size.width() * 0.35, size.height() * 0.32, size.width() * 0.18, size.height() * 0.13));

    painter.setPen(QPen(QColor(70, 45, 25), 1.6));
    painter.drawArc(QRectF(size.width() * 0.35, size.height() * 0.48, size.width() * 0.30, size.height() * 0.20), 0, 180 * 16);
}

void drawQueen(QPainter& painter, const QSize& size, const QColor& color)
{
    drawColony(painter, size, color);

    QPolygonF crown;
    crown << p(size, 0.30, 0.28)
          << p(size, 0.34, 0.10)
          << p(size, 0.44, 0.22)
          << p(size, 0.50, 0.07)
          << p(size, 0.57, 0.22)
          << p(size, 0.68, 0.10)
          << p(size, 0.71, 0.28);

    painter.setPen(QPen(QColor(110, 75, 15), 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(250, 205, 45));
    painter.drawPolygon(crown);
    painter.drawRoundedRect(
        QRectF(
            size.width() * 0.30,
            size.height() * 0.25,
            size.width() * 0.41,
            size.height() * 0.10
            ),
        1.5,
        1.5
        );
}

void drawRandom(QPainter& painter, const QSize& size)
{
    QRectF r(size.width() * 0.25, size.height() * 0.25, size.width() * 0.50, size.height() * 0.50);

    painter.setPen(QPen(QColor(230, 235, 240), 1.5));
    painter.setBrush(QColor(80, 95, 115));
    painter.drawRoundedRect(r, 4, 4);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(245, 245, 245));

    const QPointF dots[] = {
        p(size, 0.38, 0.38),
        p(size, 0.62, 0.38),
        p(size, 0.50, 0.50),
        p(size, 0.38, 0.62),
        p(size, 0.62, 0.62)
    };

    for (const QPointF& d : dots) {
        painter.drawEllipse(d, 1.7, 1.7);
    }
}

void drawOpen(QPainter& painter, const QSize& size)
{
    painter.setPen(QPen(QColor(95, 70, 30), 1.4));
    painter.setBrush(QColor(210, 160, 65));

    QPolygonF folder;
    folder << p(size, 0.16, 0.36)
           << p(size, 0.40, 0.36)
           << p(size, 0.46, 0.28)
           << p(size, 0.84, 0.28)
           << p(size, 0.84, 0.74)
           << p(size, 0.16, 0.74);

    painter.drawPolygon(folder);

    painter.setBrush(QColor(235, 190, 80));
    painter.drawRoundedRect(QRectF(size.width() * 0.14, size.height() * 0.42, size.width() * 0.72, size.height() * 0.34), 3, 3);
}

void drawSave(QPainter& painter, const QSize& size)
{
    painter.setPen(QPen(QColor(35, 45, 55), 1.4));
    painter.setBrush(QColor(70, 100, 125));
    painter.drawRoundedRect(QRectF(size.width() * 0.20, size.height() * 0.16, size.width() * 0.60, size.height() * 0.68), 3, 3);

    painter.setBrush(QColor(230, 235, 240));
    painter.drawRect(QRectF(size.width() * 0.30, size.height() * 0.20, size.width() * 0.32, size.height() * 0.18));

    painter.setBrush(QColor(35, 45, 55));
    painter.drawRect(QRectF(size.width() * 0.32, size.height() * 0.56, size.width() * 0.36, size.height() * 0.20));
}

void drawPlay(QPainter& painter, const QSize& size)
{
    QPolygonF play;
    play << p(size, 0.33, 0.22)
         << p(size, 0.33, 0.78)
         << p(size, 0.78, 0.50);

    painter.setPen(QPen(QColor(25, 80, 35), 1.5));
    painter.setBrush(QColor(90, 210, 90));
    painter.drawPolygon(play);
}

void drawPause(QPainter& painter, const QSize& size)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(230, 235, 240));
    painter.drawRoundedRect(QRectF(size.width() * 0.32, size.height() * 0.22, size.width() * 0.12, size.height() * 0.56), 2, 2);
    painter.drawRoundedRect(QRectF(size.width() * 0.56, size.height() * 0.22, size.width() * 0.12, size.height() * 0.56), 2, 2);
}

void drawSize(QPainter& painter, const QSize& size, double factor)
{
    QRectF r = unitRect(size, 4);
    painter.setPen(QPen(QColor(210, 220, 230), 1.4));
    painter.setBrush(QColor(80, 95, 115));

    const double w = r.width() * factor;
    const double h = r.height() * factor;

    painter.drawRect(QRectF(
        r.center().x() - w * 0.5,
        r.center().y() - h * 0.5,
        w,
        h
        ));
}

void drawSpiderHole(QPainter& painter, const QSize& size)
{
    const QPointF center = p(size, 0.50, 0.55);

    painter.setRenderHint(QPainter::Antialiasing, true);

    // Erdwall
    painter.setPen(QPen(QColor(55, 30, 18), 1.4));
    painter.setBrush(QColor(105, 62, 34));

    painter.drawEllipse(
        QRectF(
            size.width() * 0.18,
            size.height() * 0.30,
            size.width() * 0.64,
            size.height() * 0.42
            )
        );

    // Loch
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(18, 12, 10));

    painter.drawEllipse(
        QRectF(
            size.width() * 0.28,
            size.height() * 0.40,
            size.width() * 0.44,
            size.height() * 0.24
            )
        );

    // Kleine Spinnenbeine/Spuren
    painter.setPen(QPen(QColor(25, 18, 15), 1.3));

    painter.drawLine(p(size, 0.35, 0.46), p(size, 0.20, 0.34));
    painter.drawLine(p(size, 0.65, 0.46), p(size, 0.80, 0.34));
    painter.drawLine(p(size, 0.35, 0.59), p(size, 0.20, 0.72));
    painter.drawLine(p(size, 0.65, 0.59), p(size, 0.80, 0.72));

    // Kleiner dunkler Punkt als Tiefe
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 120));
    painter.drawEllipse(center, 3.0, 2.0);
}

}

QIcon IconFactory::icon(Icon iconType, const QSize& size)
{
    return QIcon(pixmap(iconType, size));
}

QPushButton* IconFactory::makeButton(
    Icon iconType,
    QWidget* parent,
    bool checkable,
    const QSize& buttonSize,
    const QSize& iconSize
    )
{
    auto* button = new QPushButton(parent);

    button->setText(QString());
    button->setIcon(icon(iconType, iconSize));
    button->setIconSize(iconSize);
    button->setCheckable(checkable);
    button->setMinimumSize(buttonSize);

    return button;
}

QPixmap IconFactory::pixmap(Icon iconType, const QSize& size)
{
    QPixmap pm(size);
    pm.fill(Qt::transparent);

    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing, true);

    switch (iconType) {
    case Icon::Pointer:
        drawPointer(painter, size);
        break;

    case Icon::Magnifier:
        drawMagnifier(painter, size);
        break;

    case Icon::Food:
        drawFood(painter, size);
        break;

    case Icon::Pheromone:
        drawPheromone(painter, size);
        break;

    case Icon::Dynamite:
        drawDynamite(painter, size);
        break;

    case Icon::Ground:
        drawGround(painter, size);
        break;

    case Icon::Grass:
        drawGrass(painter, size);
        break;

    case Icon::Rock:
        drawRock(painter, size);
        break;

    case Icon::Water:
        drawWater(painter, size, false);
        break;

    case Icon::ShallowWater:
        drawWater(painter, size, true);
        break;

    case Icon::Spider:
        drawSpider(painter, size);
        break;

    case Icon::Flower:
        drawFlower(painter, size);
        break;

    case Icon::SpiderHole:
        drawSpiderHole(painter, size);
        break;

    case Icon::ColonyRed:
        drawColony(painter, size, QColor(220, 45, 45));
        break;

    case Icon::ColonyMagenta:
        drawColony(painter, size, QColor(210, 45, 210));
        break;

    case Icon::ColonyCoral:
        drawColony(painter, size, QColor(255, 128, 128));
        break;

    case Icon::QueenRed:
        drawQueen(painter, size, QColor(220, 45, 45));
        break;

    case Icon::QueenMagenta:
        drawQueen(painter, size, QColor(210, 45, 210));
        break;

    case Icon::QueenCoral:
        drawQueen(painter, size, QColor(255, 128, 128));
        break;

    case Icon::Random:
        drawRandom(painter, size);
        break;

    case Icon::Open:
        drawOpen(painter, size);
        break;

    case Icon::Save:
        drawSave(painter, size);
        break;

    case Icon::Play:
        drawPlay(painter, size);
        break;

    case Icon::SizeSmall:
        drawSize(painter, size, 0.35);
        break;

    case Icon::SizeMedium:
        drawSize(painter, size, 0.58);
        break;

    case Icon::SizeLarge:
        drawSize(painter, size, 0.82);
        break;

    case Icon::Pause:
        drawPause(painter, size);
        break;

    case Icon::Speed1:
        drawSpeedTriangle(painter, size, 1);
        break;

    case Icon::Speed2:
        drawSpeedTriangle(painter, size, 2);
        break;
    }

    painter.end();

    return pm;
}
