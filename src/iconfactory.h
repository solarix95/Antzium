#ifndef ICONFACTORY_H
#define ICONFACTORY_H

#include <QIcon>
#include <QSize>

class QPushButton;
class QWidget;

class IconFactory
{
public:
    enum class Icon
    {
        Pointer,
        Magnifier,
        Food,
        Pheromone,
        Dynamite,

        Ground,
        Grass,
        Rock,
        Water,
        ShallowWater,

        Spider,
        Flower,
        SpiderHole,
        ColonyRed,
        ColonyMagenta,
        ColonyCoral,
        QueenRed,
        QueenMagenta,
        QueenCoral,

        Random,
        Open,
        Save,
        Play,

        SizeSmall,
        SizeMedium,
        SizeLarge,

        Pause,
        Speed1,
        Speed2
    };

    static QIcon icon(Icon icon, const QSize& size = QSize(32, 32));

    static QPushButton* makeButton(
        Icon icon,
        QWidget* parent,
        bool checkable = false,
        const QSize& buttonSize = QSize(42, 34),
        const QSize& iconSize = QSize(24, 24)
        );

private:
    static QPixmap pixmap(Icon icon, const QSize& size);
};

#endif // ICONFACTORY_H
