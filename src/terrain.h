#pragma once

#include <QColor>

enum class TerrainType
{
    Ground,
    Grass,
    Rock,
    Water,
    ShallowWater,
    ColonyStart,
    Food
};

struct Tile
{
    TerrainType type = TerrainType::Ground;
    double speedFactor = 1.0;
    bool walkable = true;
    QColor renderColor = QColor(225, 210, 160);
};
