#ifndef ABSTRACTEFFECT_H
#define ABSTRACTEFFECT_H

#include <QObject>
#include <QPainter>
#include <QRectF>

class AbstractEffect : public QObject
{
public:
    explicit AbstractEffect(QObject* parent = nullptr);
    ~AbstractEffect() override = default;

    inline void tick(double dt)
    {
        m_timeElapsed += dt;
    }

    inline double timeElapsed() const
    {
        return m_timeElapsed;
    }

    virtual void drawEffect(QPainter& painter, const QRectF& cameraWorldRect) = 0;

private:
    double m_timeElapsed = 0.0;
};

#endif // ABSTRACTEFFECT_H
