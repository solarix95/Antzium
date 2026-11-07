#pragma once

#include <QPointF>

enum class SoundEffect
{
    FoodPlaced,
    DynamitePlaced,
    FlowerPlaced,
    TerrainPainted,
    Burn,
    Explosion,
    WaspKillsSpider,
    SpiderWinsDuel,
    ColonyDied,
    ColonyFounded,
    QueenSpawned,
    AmbientThunder1,
    AmbientThunder2,
    AmbientThunder3,
    AmbientCricket1,
    AmbientBird1,
    AmbientCricket2,
    AmbientBird2,
    Count
};

struct SoundEvent
{
    SoundEffect effect = SoundEffect::FoodPlaced;
    QPointF worldPosition;
};
