#pragma once

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QImage>
#include <QRandomGenerator>

#include "flowfield.h"
#include "pheromonefield.h"
#include "worldmap.h"
#include "audio/soundevent.h"
#include "effects/abstracteffect.h"

class QPainter;

struct FoodSource
{
    QPointF pos;
    int amount = 100;
};

struct Colony
{
    QPointF home;
    QColor color;
    int storedFood = 0;
    int antCount = 0;
    int maxAntCount = 1500;
    double nestHealth = 100.0;

    double highPopulationTimeSeconds = 0.0;
    bool mature = false;
    double queenSpawnCooldownSeconds = 0.0;

    bool dead = false;
    bool destroyed = false;
    double deathTimeSeconds = -1.0;
};

struct Ant
{
    int colonyId = 0;
    QPointF pos;
    QPointF dir;
    bool carryingFood = false;

    bool dead = false;
    double ageSeconds = 0.0;
    double lifeTimeSeconds = 140.0;

    // Carrying / Home recovery
    QPointF previousPos;
    double stuckTime = 0.0;

    double previousHomeCost = 0.0;
    double noProgressTime = 0.0;

    bool recovering = false;
    double recoveryTime = 0.0;
    QPointF recoveryDir;

    // Searching / Food recovery
    int searchTargetFoodIndex = -1;
    QPointF searchPreviousPos;
    double searchStuckTime = 0.0;

    double previousFoodDistance = 0.0;
    double foodNoProgressTime = 0.0;

    bool searchRecovering = false;
    double searchRecoveryTime = 0.0;
    QPointF searchRecoveryDir;

    // dont think every tick()
    double thinkTimer = 0.0;
    double thinkInterval = 0.10; // z. B. 10x per sec
};
struct Spider
{
    QPointF pos;
    QPointF dir;

    double wanderTime = 0.0;
    double attackCooldown = 0.0;

    int eatenAnts = 0;

    // Reproduction
    int eatenSinceLastOffspring = 0;
    double reproductionCooldown = 0.0;

    // Lifetime
    bool dead = false;
    double ageSeconds = 0.0;
    double lifeTimeSeconds = 100.0;

    // Digestion
    QVector<double> digestionSlots;
};

struct Flower
{
    QPointF pos;
    QColor petalColor;

    double spawnTimer = 0.0;
    double spawnInterval = 30.0;

    int foodPerSpawn = 18;
    int maxNearbyFood = 180;

    double scatterRadius = 38.0;
    double visualPhase = 0.0;

    inline void randomizeSpawnInterval()
    {
        spawnInterval =
            30.0 + QRandomGenerator::global()->generateDouble() * 40.0;
    }

    inline void startInitialSpawnInterval()
    {
        randomizeSpawnInterval();
        spawnTimer =
            QRandomGenerator::global()->generateDouble() * spawnInterval;
    }

    inline void restartSpawnInterval()
    {
        randomizeSpawnInterval();
        spawnTimer = spawnInterval;
    }
};

struct Wasp
{
    QPointF pos;
    QPointF dir;

    bool dead = false;

    double ageSeconds = 0.0;
    double lifeTimeSeconds = 45.0;

    double wanderTime = 0.0;
    double attackCooldown = 0.0;

    double wingPhase = 0.0;

    int killedSpiders = 0;
};

struct Queen
{
    QPointF pos;
    QPointF dir;
    QColor color;
    bool dead = false;

    double wingPhase = 0.0;
    double wanderTime = 0.0;
    double flightTime = 0.0;
    double explorationDurationSeconds = 45.0;
    double surveyTimer = 0.0;

    bool hasBestFoodPosition = false;
    QPointF bestFoodPos;
    int bestFoodAmount = 0;
    bool hasFoundingSite = false;
    QPointF foundingSitePos;
    bool returningToBest = false;
};

struct SpiderHole
{
    QPointF pos;
    double health = 100.0;

    double spawnTimer = 0.0;
    double spawnInterval = 90.0;

    double localRadius = 180.0;
    int maxLocalSpiders = 5;

    double visualPhase = 0.0;
};

struct DynamiteCharge
{
    QPointF pos;
    QColor groundColor;

    bool ignited = false;
    bool exploded = false;

    double fuseTime = 0.0;
    double fuseDuration = 1.15;
    double visualPhase = 0.0;
};

class GameWorld
{
public:
    ~GameWorld();

    bool loadMap(const QString& fileName, int tileSize);
    bool loadMapFromImage(const QImage& image, int tileSize);
    bool saveBaseMapPng(const QString& fileName) const;
    bool saveStateJson(const QString& fileName) const;
    bool loadStateJson(const QString& fileName);
    void createFallback();

    bool hasMap() const;
    const WorldMap& map() const;
    double worldTimeSeconds() const;
    quint64 terrainRevision() const;

    int    colonyCount() const;
    int    totalAntCount() const;
    QColor colonyColor(int colonyId) const;
    bool   setColonyColor(int colonyId, const QColor& color);
    int    antCountForColony(int colonyId) const;
    int    colonyStoredFood(int colonyId) const;
    int    colonyScore(int colonyId) const;
    QPointF colonyHome(int colonyId) const;
    bool   colonyIsDead(int colonyId) const;
    bool   colonyIsDestroyed(int colonyId) const;
    double colonyNestHealth(int colonyId) const;
    double colonyDeathTimeSeconds(int colonyId) const;

    int    spiderCount() const;
    void   resolveSpiderDuels();
    double spiderDuelStrength(const Spider& spider) const;
    void   killSpiderAsFood(Spider& spider);

    int waspCount() const;

    void setWeatherSpeedFactor(double factor);
    void tick(double dt);
    void tickEffects(double dt);
    QVector<SoundEvent> takeSoundEvents();

    void drawEntities(QPainter& painter, const QRectF& cameraWorldRect) const;

    // Userinteraction
    void burnAt(const QPointF& worldPos, double radiusPx);
    void placeFoodAt(const QPointF& worldPos, int amount);
    void placeFlowerAt(const QPointF& worldPos);
    void placePheromoneAt(const QPointF& worldPos, double radiusPx, double amount);
    void placeDynamiteAt(const QPointF& worldPos);
    void placeGrassAt(const QPointF& worldPos, double radiusPx);
    void placeRockAt(const QPointF& worldPos, double radiusPx);

    void registerEffect(AbstractEffect* effect);

private: // Flower
    void spawnFlowerAt(const QPointF& pos);
    void handleFlower(Flower& flower, double dt);
    int foodAmountNear(const QPointF& pos, double radiusPx) const;
    QColor randomFlowerColor() const;

private: // Wasp
    void spawnWaspAt(const QPointF& pos);
    void handleWasp(Wasp& wasp, double dt);
    void removeDeadWasps();
    void killWaspAsFood(Wasp& wasp);
    bool isReachableByAnt(const QPointF& pos) const;

    int nearestLiveSpiderIndex(const QPointF& pos, double radiusPx) const;

    QPointF steerWasp(
        const Wasp& wasp,
        const QPointF& desiredDir
        ) const;

    void keepWaspInsideWorld(Wasp& wasp);

    void drawWasps(QPainter& painter, const QRectF& cameraWorldRect) const;

private: // Queen
    void spawnQueenAt(const QPointF& pos, const QColor& color);
    void updateColonyMaturity(double dt);
    bool handleQueen(Queen& queen, double dt);
    bool foundColonyFromQueen(const Queen& queen);
    bool findQueenFoundingSite(const QPointF& favoritePos, QPointF& sitePos) const;
    bool isQueenFoundingSiteValid(const QPointF& pos) const;
    QPointF steerQueen(const Queen& queen, const QPointF& desiredDir) const;
    void keepQueenInsideWorld(Queen& queen);
    void updateQueenFoodMemory(Queen& queen);
    void drawQueens(QPainter& painter, const QRectF& cameraWorldRect) const;

private: // SpiderHole
    void spawnSpiderHoleAt(const QPointF& pos);
    void handleSpiderHole(SpiderHole& hole, double dt);

    int spiderCountNear(const QPointF& pos, double radiusPx) const;

    void drawSpiderHoles(QPainter& painter, const QRectF& cameraWorldRect) const;

private: // Dynamite
    void handleDynamite(double dt);
    void igniteDynamiteNear(const QPointF& pos, double radiusPx, double delaySeconds);
    void igniteDynamiteFromExplosion(const QPointF& pos, double radiusPx);
    void explodeDynamite(int index);
    void destroyEverythingInCircle(const QPointF& pos, double radiusPx);
    void damageNestsAt(const QPointF& pos, double radiusPx);
    void removeExplodedDynamite();
    QColor explosionColorForTerrain(TerrainType type, const QColor& renderColor) const;
    void drawDynamite(QPainter& painter, const QRectF& cameraWorldRect) const;

private:
    void rebuildFromMap();
    void rebuildHomeFlowFields();

    // Rendering
    void drawFood(QPainter& painter, const QRectF& cameraWorldRect) const;
    void drawColonies(QPainter& painter, const QRectF& cameraWorldRect) const;
    void drawAnts(QPainter& painter, const QRectF& cameraWorldRect) const;
    void drawSpiders(QPainter& painter, const QRectF& cameraWorldRect) const;
    void drawFlowers(QPainter& painter, const QRectF& cameraWorldRect) const;
    void drawHudText(QPainter& painter, const QRectF& cameraWorldRect) const;
    void drawEffects(QPainter& painter, const QRectF& cameraWorldRect) const;

    void spawnAnt(int colonyId);
    void handleAnt(Ant& ant, double dt);

    void handleCarryingAnt(Ant& ant, Colony& colony, double dt, bool thinkNow);
    void handleSearchingAnt(Ant& ant, Colony& colony, double dt, bool thinkNow);

    bool updateAntThinkTimer(Ant& ant, double dt);
    void resetAntThinkSchedule(Ant& ant);

    void moveAntCheap(
        Ant& ant,
        double baseSpeed,
        double dt,
        const QPointF& fallbackDir
        );

    void updateCarryingRecoveryState(Ant& ant, double dt);
    void beginRecovery(Ant& ant);
    void resetNavigationMemory(Ant& ant);

    QPointF homeDirectionForAnt(const Ant& ant, const Colony& colony) const;
    double homeCostForAnt(const Ant& ant) const;

    QPointF steerWithFeelers(
        const Ant& ant,
        PheromoneKind kind,
        const QPointF& desiredDir,
        double desiredWeight,
        double pheromoneWeight
        ) const;

    void updateSearchingRecoveryState(
        Ant& ant,
        bool hasFoodTarget,
        const QPointF& foodTargetPos,
        int foodTargetIndex,
        double dt
        );

    void beginSearchRecovery(Ant& ant, const QPointF& desiredDir);
    void resetSearchMemory(Ant& ant);

    QPointF chooseSearchRecoveryDirection(
        const Ant& ant,
        const QPointF& desiredDir
        ) const;

    bool tryCollectFoodAtIndex(
        Ant& ant,
        int foodIndex,
        double radiusPx
        );

    QPointF steerToCloseFood(
        const Ant& ant,
        const QPointF& desiredDir,
        double foodDistance
        ) const;

    int nearestVisibleFoodIndex(const QPointF& pos, double radiusPx) const;

    bool hasWalkableLine(
        const QPointF& from,
        const QPointF& to,
        double maxDistance,
        double stepPx
        ) const;

    QPointF chooseRecoveryDirection(const Ant& ant) const;
    QPointF avoidObstacleDirection(const Ant& ant, const QPointF& desiredDir) const;

    bool tryMoveAnt(Ant& ant, const QPointF& dir, double step) const;
    bool canMoveInDirection(const QPointF& pos, const QPointF& dir, double distance) const;

    int nearestFoodIndex(const QPointF& pos, double radiusPx) const;

    QPointF randomDirection() const;
    QPointF normalized(const QPointF& v) const;
    QPointF blendDirections(const QPointF& a, double weightA, const QPointF& b, double weightB) const;
    QPointF rotated(const QPointF& v, double radians) const;

    double dot(const QPointF& a, const QPointF& b) const;
    double distanceSquared(const QPointF& a, const QPointF& b) const;
    double distance(const QPointF& a, const QPointF& b) const;

private:
    void spawnSpiderAt(const QPointF& pos);
    void handleSpider(Spider& spider, double dt);
    void removeDeadAnts();
    void updateSpiderDigestion(Spider& spider, double dt);
    bool spiderIsHungry(const Spider& spider) const;
    bool spiderEatAnt(Spider& spider, Ant& ant);
    int spiderUsedSlots(const Spider& spider) const;
    double spiderFullnessRatio(const Spider& spider) const;
    double spiderSpeedFactor(const Spider& spider) const;

    int nearestLiveAntIndex(const QPointF& pos, double radiusPx) const;

    QPointF steerSpider(
        const Spider& spider,
        const QPointF& desiredDir
        ) const;

    bool tryMoveSpider(
        Spider& spider,
        const QPointF& dir,
        double step
        ) const;

    void spawnFoodAt(const QPointF& pos, int amount);
    void removeDepletedFoodSources();
    void removeDeadSpiders();

    bool spiderWantsToReproduce(const Spider& spider) const;
    QPointF findNearbyWalkablePosition(
        const QPointF& origin,
        double minRadius,
        double maxRadius
        ) const;

private:
    void updateNestHealth(double dt);
    void updateColonyDeathStates();
    void removeExpiredColonies();

    WorldMap m_map;
    quint64 m_terrainRevision = 0;
    double   m_worldTimeSeconds = 0.0;
    double   m_weatherSpeedFactor = 1.0;

    QVector<Colony> m_colonies;
    QVector<Ant> m_ants;
    QVector<FoodSource> m_food;
    QVector<Spider> m_spiders;
    QVector<Flower> m_flowers;
    QVector<Wasp> m_wasps;
    QVector<Queen> m_queens;
    QVector<SpiderHole> m_spiderHoles;
    QVector<DynamiteCharge> m_dynamite;
    QVector<SoundEvent> m_soundEvents;

    QVector<AbstractEffect*> m_effects;

    PheromoneField m_pheromones;

    // Eine Heimweg-Navigation pro Kolonie.
    QVector<FlowField> m_homeFlowFields;
};
