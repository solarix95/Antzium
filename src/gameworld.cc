
#include <algorithm>
#include <cmath>
#include <limits>

#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QBuffer>
#include <QPainter>
#include <QRandomGenerator>

#include "gameworld.h"
#include "effects/anteateneffect.h"
#include "effects/burneffect.h"
#include "effects/explosioneffect.h"

namespace
{
constexpr double Pi = 3.14159265358979323846;
constexpr double Infinity = std::numeric_limits<double>::infinity();
constexpr double SpiderEatCooldownSeconds = 5.0;
constexpr int MaximumSpiderCount = 64;
constexpr int QueenFoundingAntCount = 10;
constexpr int ColonyMaturityPopulation = 300;
constexpr double ColonyMaturityDurationSeconds = 180.0;
constexpr double QueenSpawnIntervalMinSeconds = 150.0;
constexpr double QueenSpawnIntervalRangeSeconds = 60.0;
constexpr double ColonyCleanupDelaySeconds = 60.0;
constexpr int QueenMinimumFoodForColony = 5 * 18;
constexpr double MaximumNestHealth = 100.0;
constexpr double MaximumNestDamagePerExplosion = 10.0;
constexpr double NestRepairPerSecond = 0.1;
constexpr double ColonyNestRadius = 17.0;
constexpr double SpiderHoleRadius = 13.0;

double explosionDamageAtDistance(
    double centerDistance,
    double targetRadius,
    double explosionRadius
    )
{
    if (explosionRadius <= 0.0) {
        return 0.0;
    }

    const double edgeDistance = std::max(0.0, centerDistance - targetRadius);
    if (edgeDistance >= explosionRadius) {
        return 0.0;
    }

    return MaximumNestDamagePerExplosion *
           (1.0 - edgeDistance / explosionRadius);
}

void drawNestHealthBar(
    QPainter& painter,
    const QPointF& pos,
    double health,
    double verticalOffset
    )
{
    if (health >= MaximumNestHealth) {
        return;
    }

    const double ratio = std::clamp(health / MaximumNestHealth, 0.0, 1.0);
    const QRectF background(pos.x() - 14.0, pos.y() + verticalOffset, 28.0, 4.0);
    const QRectF fill(
        background.left() + 1.0,
        background.top() + 1.0,
        (background.width() - 2.0) * ratio,
        background.height() - 2.0
        );

    painter.save();
    painter.setPen(QPen(QColor(20, 20, 20, 210), 0.8));
    painter.setBrush(QColor(35, 25, 25, 190));
    painter.drawRoundedRect(background, 1.5, 1.5);

    if (ratio > 0.0) {
        const int hue = static_cast<int>(std::round(120.0 * ratio));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor::fromHsv(hue, 210, 225));
        painter.drawRect(fill);
    }

    painter.restore();
}
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

GameWorld::~GameWorld()
{
    QVector<AbstractEffect*> effects = m_effects;
    m_effects.clear();

    qDeleteAll(effects);
}

bool GameWorld::loadMap(const QString& fileName, int tileSize)
{
    if (!m_map.loadFromPng(fileName, tileSize)) {
        return false;
    }

    rebuildFromMap();
    return true;
}

bool GameWorld::loadMapFromImage(const QImage& image, int tileSize)
{
    if (!m_map.loadFromImage(image, tileSize)) {
        return false;
    }

    rebuildFromMap();
    return true;
}


namespace
{
QJsonObject pointToJson(const QPointF& p)
{
    QJsonObject o;
    o[QStringLiteral("x")] = p.x();
    o[QStringLiteral("y")] = p.y();
    return o;
}

QPointF pointFromJson(const QJsonObject& o, const QPointF& fallback = QPointF())
{
    return QPointF(
        o[QStringLiteral("x")].toDouble(fallback.x()),
        o[QStringLiteral("y")].toDouble(fallback.y())
        );
}

QJsonObject colorToJson(const QColor& c)
{
    QJsonObject o;
    o[QStringLiteral("r")] = c.red();
    o[QStringLiteral("g")] = c.green();
    o[QStringLiteral("b")] = c.blue();
    o[QStringLiteral("a")] = c.alpha();
    return o;
}

QColor colorFromJson(const QJsonObject& o, const QColor& fallback = QColor(255, 255, 255))
{
    return QColor(
        o[QStringLiteral("r")].toInt(fallback.red()),
        o[QStringLiteral("g")].toInt(fallback.green()),
        o[QStringLiteral("b")].toInt(fallback.blue()),
        o[QStringLiteral("a")].toInt(fallback.alpha())
        );
}

QPoint tilePointForWorld(const WorldMap& map, const QPointF& pos)
{
    const int tileSize = std::max(1, map.tileSize());
    return QPoint(
        std::clamp(static_cast<int>(std::floor(pos.x() / tileSize)), 0, map.widthTiles() - 1),
        std::clamp(static_cast<int>(std::floor(pos.y() / tileSize)), 0, map.heightTiles() - 1)
        );
}

QColor colonyMarkerColor(const QColor& color)
{
    if (color.green() > 80 && color.blue() > 80) {
        return QColor(255, 128, 128);
    }

    if (color.blue() > 150) {
        return QColor(255, 0, 255);
    }

    return QColor(255, 0, 0);
}

QColor queenMarkerColor(const QColor& color)
{
    if (color.green() > 80 && color.blue() > 80) {
        return QColor(128, 64, 64);
    }

    if (color.blue() > 150) {
        return QColor(128, 0, 128);
    }

    return QColor(128, 0, 0);
}
}

bool GameWorld::saveBaseMapPng(const QString& fileName) const
{
    if (!hasMap() || fileName.isEmpty()) {
        return false;
    }

    QImage image = m_map.toEditorImage();

    if (image.isNull()) {
        return false;
    }

    for (const Colony& colony : m_colonies) {
        if (colony.destroyed) {
            continue;
        }

        const QPoint p = tilePointForWorld(m_map, colony.home);
        image.setPixelColor(p, colonyMarkerColor(colony.color));
    }

    for (const Queen& queen : m_queens) {
        const QPoint p = tilePointForWorld(m_map, queen.pos);
        image.setPixelColor(p, queenMarkerColor(queen.color));
    }

    for (const FoodSource& food : m_food) {
        if (food.amount <= 0) {
            continue;
        }

        const QPoint p = tilePointForWorld(m_map, food.pos);
        image.setPixelColor(p, QColor(255, 255, 0));
    }

    for (const Flower& flower : m_flowers) {
        const QPoint p = tilePointForWorld(m_map, flower.pos);
        image.setPixelColor(p, QColor(128, 0, 255));
    }

    for (const SpiderHole& hole : m_spiderHoles) {
        if (hole.health <= 0.0) {
            continue;
        }

        const QPoint p = tilePointForWorld(m_map, hole.pos);
        image.setPixelColor(p, QColor(80, 40, 20));
    }

    return image.save(fileName, "PNG");
}

bool GameWorld::saveStateJson(const QString& fileName) const
{
    if (!hasMap() || fileName.isEmpty()) {
        return false;
    }

    QImage mapImage = m_map.toEditorImage();
    QByteArray mapBytes;
    QBuffer buffer(&mapBytes);
    buffer.open(QIODevice::WriteOnly);
    mapImage.save(&buffer, "PNG");

    QJsonObject root;
    root[QStringLiteral("format")] = QStringLiteral("AntSimSave");
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("worldTimeSeconds")] = m_worldTimeSeconds;
    root[QStringLiteral("tileSize")] = m_map.tileSize();
    root[QStringLiteral("mapPngBase64")] = QString::fromLatin1(mapBytes.toBase64());

    QJsonArray colonies;
    for (const Colony& c : m_colonies) {
        QJsonObject o;
        o[QStringLiteral("home")] = pointToJson(c.home);
        o[QStringLiteral("color")] = colorToJson(c.color);
        o[QStringLiteral("storedFood")] = c.storedFood;
        o[QStringLiteral("antCount")] = c.antCount;
        o[QStringLiteral("maxAntCount")] = c.maxAntCount;
        o[QStringLiteral("nestHealth")] = c.nestHealth;
        o[QStringLiteral("highPopulationTimeSeconds")] = c.highPopulationTimeSeconds;
        o[QStringLiteral("mature")] = c.mature;
        o[QStringLiteral("queenSpawnCooldownSeconds")] = c.queenSpawnCooldownSeconds;
        o[QStringLiteral("dead")] = c.dead;
        o[QStringLiteral("destroyed")] = c.destroyed;
        o[QStringLiteral("deathTimeSeconds")] = c.deathTimeSeconds;
        colonies.push_back(o);
    }
    root[QStringLiteral("colonies")] = colonies;

    QJsonArray ants;
    for (const Ant& a : m_ants) {
        QJsonObject o;
        o[QStringLiteral("colonyId")] = a.colonyId;
        o[QStringLiteral("pos")] = pointToJson(a.pos);
        o[QStringLiteral("dir")] = pointToJson(a.dir);
        o[QStringLiteral("carryingFood")] = a.carryingFood;
        o[QStringLiteral("dead")] = a.dead;
        o[QStringLiteral("ageSeconds")] = a.ageSeconds;
        o[QStringLiteral("lifeTimeSeconds")] = a.lifeTimeSeconds;
        o[QStringLiteral("previousPos")] = pointToJson(a.previousPos);
        o[QStringLiteral("stuckTime")] = a.stuckTime;
        o[QStringLiteral("previousHomeCost")] = a.previousHomeCost;
        o[QStringLiteral("noProgressTime")] = a.noProgressTime;
        o[QStringLiteral("recovering")] = a.recovering;
        o[QStringLiteral("recoveryTime")] = a.recoveryTime;
        o[QStringLiteral("recoveryDir")] = pointToJson(a.recoveryDir);
        o[QStringLiteral("searchTargetFoodIndex")] = a.searchTargetFoodIndex;
        o[QStringLiteral("searchPreviousPos")] = pointToJson(a.searchPreviousPos);
        o[QStringLiteral("searchStuckTime")] = a.searchStuckTime;
        o[QStringLiteral("previousFoodDistance")] = a.previousFoodDistance;
        o[QStringLiteral("foodNoProgressTime")] = a.foodNoProgressTime;
        o[QStringLiteral("searchRecovering")] = a.searchRecovering;
        o[QStringLiteral("searchRecoveryTime")] = a.searchRecoveryTime;
        o[QStringLiteral("searchRecoveryDir")] = pointToJson(a.searchRecoveryDir);
        o[QStringLiteral("thinkTimer")] = a.thinkTimer;
        o[QStringLiteral("thinkInterval")] = a.thinkInterval;
        ants.push_back(o);
    }
    root[QStringLiteral("ants")] = ants;

    QJsonArray food;
    for (const FoodSource& f : m_food) {
        QJsonObject o;
        o[QStringLiteral("pos")] = pointToJson(f.pos);
        o[QStringLiteral("amount")] = f.amount;
        food.push_back(o);
    }
    root[QStringLiteral("food")] = food;

    QJsonArray spiders;
    for (const Spider& sp : m_spiders) {
        QJsonObject o;
        o[QStringLiteral("pos")] = pointToJson(sp.pos);
        o[QStringLiteral("dir")] = pointToJson(sp.dir);
        o[QStringLiteral("wanderTime")] = sp.wanderTime;
        o[QStringLiteral("attackCooldown")] = sp.attackCooldown;
        o[QStringLiteral("eatenAnts")] = sp.eatenAnts;
        o[QStringLiteral("eatenSinceLastOffspring")] = sp.eatenSinceLastOffspring;
        o[QStringLiteral("reproductionCooldown")] = sp.reproductionCooldown;
        o[QStringLiteral("dead")] = sp.dead;
        o[QStringLiteral("ageSeconds")] = sp.ageSeconds;
        o[QStringLiteral("lifeTimeSeconds")] = sp.lifeTimeSeconds;
        QJsonArray digestionSlotValues;
        for (double slot : sp.digestionSlots) digestionSlotValues.push_back(slot);
        o[QStringLiteral("digestionSlots")] = digestionSlotValues;
        spiders.push_back(o);
    }
    root[QStringLiteral("spiders")] = spiders;

    QJsonArray flowers;
    for (const Flower& fl : m_flowers) {
        QJsonObject o;
        o[QStringLiteral("pos")] = pointToJson(fl.pos);
        o[QStringLiteral("petalColor")] = colorToJson(fl.petalColor);
        o[QStringLiteral("spawnTimer")] = fl.spawnTimer;
        o[QStringLiteral("spawnInterval")] = fl.spawnInterval;
        o[QStringLiteral("foodPerSpawn")] = fl.foodPerSpawn;
        o[QStringLiteral("maxNearbyFood")] = fl.maxNearbyFood;
        o[QStringLiteral("scatterRadius")] = fl.scatterRadius;
        o[QStringLiteral("visualPhase")] = fl.visualPhase;
        flowers.push_back(o);
    }
    root[QStringLiteral("flowers")] = flowers;

    QJsonArray wasps;
    for (const Wasp& w : m_wasps) {
        QJsonObject o;
        o[QStringLiteral("pos")] = pointToJson(w.pos);
        o[QStringLiteral("dir")] = pointToJson(w.dir);
        o[QStringLiteral("dead")] = w.dead;
        o[QStringLiteral("ageSeconds")] = w.ageSeconds;
        o[QStringLiteral("lifeTimeSeconds")] = w.lifeTimeSeconds;
        o[QStringLiteral("wanderTime")] = w.wanderTime;
        o[QStringLiteral("attackCooldown")] = w.attackCooldown;
        o[QStringLiteral("wingPhase")] = w.wingPhase;
        o[QStringLiteral("killedSpiders")] = w.killedSpiders;
        wasps.push_back(o);
    }
    root[QStringLiteral("wasps")] = wasps;

    QJsonArray queens;
    for (const Queen& q : m_queens) {
        QJsonObject o;
        o[QStringLiteral("pos")] = pointToJson(q.pos);
        o[QStringLiteral("dir")] = pointToJson(q.dir);
        o[QStringLiteral("color")] = colorToJson(q.color);
        o[QStringLiteral("dead")] = q.dead;
        o[QStringLiteral("wingPhase")] = q.wingPhase;
        o[QStringLiteral("wanderTime")] = q.wanderTime;
        o[QStringLiteral("flightTime")] = q.flightTime;
        o[QStringLiteral("explorationDurationSeconds")] = q.explorationDurationSeconds;
        o[QStringLiteral("surveyTimer")] = q.surveyTimer;
        o[QStringLiteral("hasBestFoodPosition")] = q.hasBestFoodPosition;
        o[QStringLiteral("bestFoodPos")] = pointToJson(q.bestFoodPos);
        o[QStringLiteral("bestFoodAmount")] = q.bestFoodAmount;
        o[QStringLiteral("hasFoundingSite")] = q.hasFoundingSite;
        o[QStringLiteral("foundingSitePos")] = pointToJson(q.foundingSitePos);
        o[QStringLiteral("returningToBest")] = q.returningToBest;
        queens.push_back(o);
    }
    root[QStringLiteral("queens")] = queens;

    QJsonArray holes;
    for (const SpiderHole& h : m_spiderHoles) {
        QJsonObject o;
        o[QStringLiteral("pos")] = pointToJson(h.pos);
        o[QStringLiteral("health")] = h.health;
        o[QStringLiteral("spawnTimer")] = h.spawnTimer;
        o[QStringLiteral("spawnInterval")] = h.spawnInterval;
        o[QStringLiteral("localRadius")] = h.localRadius;
        o[QStringLiteral("maxLocalSpiders")] = h.maxLocalSpiders;
        o[QStringLiteral("visualPhase")] = h.visualPhase;
        holes.push_back(o);
    }
    root[QStringLiteral("spiderHoles")] = holes;

    QJsonArray dynamite;
    for (const DynamiteCharge& d : m_dynamite) {
        QJsonObject o;
        o[QStringLiteral("pos")] = pointToJson(d.pos);
        o[QStringLiteral("groundColor")] = colorToJson(d.groundColor);
        o[QStringLiteral("ignited")] = d.ignited;
        o[QStringLiteral("exploded")] = d.exploded;
        o[QStringLiteral("fuseTime")] = d.fuseTime;
        o[QStringLiteral("fuseDuration")] = d.fuseDuration;
        o[QStringLiteral("visualPhase")] = d.visualPhase;
        dynamite.push_back(o);
    }
    root[QStringLiteral("dynamite")] = dynamite;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool GameWorld::loadStateJson(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return false;
    }

    const QJsonObject root = doc.object();
    if (root[QStringLiteral("format")].toString() != QStringLiteral("AntSimSave")) {
        return false;
    }

    const QByteArray mapBytes = QByteArray::fromBase64(root[QStringLiteral("mapPngBase64")].toString().toLatin1());
    QImage mapImage;
    if (!mapImage.loadFromData(mapBytes, "PNG")) {
        return false;
    }

    const int tileSize = root[QStringLiteral("tileSize")].toInt(10);
    if (!m_map.loadFromImage(mapImage, tileSize)) {
        return false;
    }

    QVector<AbstractEffect*> effects = m_effects;
    m_effects.clear();
    qDeleteAll(effects);

    m_worldTimeSeconds = root[QStringLiteral("worldTimeSeconds")].toDouble(0.0);
    m_colonies.clear();
    m_ants.clear();
    m_food.clear();
    m_spiders.clear();
    m_flowers.clear();
    m_wasps.clear();
    m_queens.clear();
    m_spiderHoles.clear();
    m_dynamite.clear();
    m_soundEvents.clear();

    for (const QJsonValue& v : root[QStringLiteral("colonies")].toArray()) {
        const QJsonObject o = v.toObject();
        Colony c;
        c.home = pointFromJson(o[QStringLiteral("home")].toObject());
        c.color = colorFromJson(o[QStringLiteral("color")].toObject(), QColor(255, 0, 0));
        c.storedFood = o[QStringLiteral("storedFood")].toInt();
        c.antCount = o[QStringLiteral("antCount")].toInt();
        c.maxAntCount = o[QStringLiteral("maxAntCount")].toInt(1500);
        c.highPopulationTimeSeconds =
            o[QStringLiteral("highPopulationTimeSeconds")].toDouble();
        c.mature = o[QStringLiteral("mature")].toBool();
        c.queenSpawnCooldownSeconds =
            o[QStringLiteral("queenSpawnCooldownSeconds")].toDouble();
        c.dead = o[QStringLiteral("dead")].toBool();
        c.destroyed = o[QStringLiteral("destroyed")].toBool();
        c.nestHealth = std::clamp(
            o[QStringLiteral("nestHealth")].toDouble(
                c.destroyed ? 0.0 : MaximumNestHealth
                ),
            0.0,
            MaximumNestHealth
            );
        if (c.nestHealth <= 0.0) {
            c.destroyed = true;
        }
        c.deathTimeSeconds = o[QStringLiteral("deathTimeSeconds")].toDouble(-1.0);
        m_colonies.push_back(c);
    }

    rebuildHomeFlowFields();
    m_pheromones.reset(m_map.widthTiles(), m_map.heightTiles(), m_map.tileSize());

    for (const QJsonValue& v : root[QStringLiteral("ants")].toArray()) {
        const QJsonObject o = v.toObject();
        Ant a;
        a.colonyId = o[QStringLiteral("colonyId")].toInt();
        a.pos = pointFromJson(o[QStringLiteral("pos")].toObject());
        a.dir = pointFromJson(o[QStringLiteral("dir")].toObject(), QPointF(1.0, 0.0));
        a.carryingFood = o[QStringLiteral("carryingFood")].toBool();
        a.dead = o[QStringLiteral("dead")].toBool();
        a.ageSeconds = o[QStringLiteral("ageSeconds")].toDouble();
        a.lifeTimeSeconds = o[QStringLiteral("lifeTimeSeconds")].toDouble(140.0);
        a.previousPos = pointFromJson(o[QStringLiteral("previousPos")].toObject(), a.pos);
        a.stuckTime = o[QStringLiteral("stuckTime")].toDouble();
        a.previousHomeCost = o[QStringLiteral("previousHomeCost")].toDouble();
        a.noProgressTime = o[QStringLiteral("noProgressTime")].toDouble();
        a.recovering = o[QStringLiteral("recovering")].toBool();
        a.recoveryTime = o[QStringLiteral("recoveryTime")].toDouble();
        a.recoveryDir = pointFromJson(o[QStringLiteral("recoveryDir")].toObject(), a.dir);
        a.searchTargetFoodIndex = o[QStringLiteral("searchTargetFoodIndex")].toInt(-1);
        a.searchPreviousPos = pointFromJson(o[QStringLiteral("searchPreviousPos")].toObject(), a.pos);
        a.searchStuckTime = o[QStringLiteral("searchStuckTime")].toDouble();
        a.previousFoodDistance = o[QStringLiteral("previousFoodDistance")].toDouble();
        a.foodNoProgressTime = o[QStringLiteral("foodNoProgressTime")].toDouble();
        a.searchRecovering = o[QStringLiteral("searchRecovering")].toBool();
        a.searchRecoveryTime = o[QStringLiteral("searchRecoveryTime")].toDouble();
        a.searchRecoveryDir = pointFromJson(o[QStringLiteral("searchRecoveryDir")].toObject(), a.dir);
        a.thinkTimer = o[QStringLiteral("thinkTimer")].toDouble();
        a.thinkInterval = o[QStringLiteral("thinkInterval")].toDouble(0.10);
        m_ants.push_back(a);
    }

    for (const QJsonValue& v : root[QStringLiteral("food")].toArray()) {
        const QJsonObject o = v.toObject();
        FoodSource f;
        f.pos = pointFromJson(o[QStringLiteral("pos")].toObject());
        f.amount = o[QStringLiteral("amount")].toInt(100);
        m_food.push_back(f);
    }

    for (const QJsonValue& v : root[QStringLiteral("spiders")].toArray()) {
        const QJsonObject o = v.toObject();
        Spider sp;
        sp.pos = pointFromJson(o[QStringLiteral("pos")].toObject());
        sp.dir = pointFromJson(o[QStringLiteral("dir")].toObject(), QPointF(1.0, 0.0));
        sp.wanderTime = o[QStringLiteral("wanderTime")].toDouble();
        sp.attackCooldown = o[QStringLiteral("attackCooldown")].toDouble();
        sp.eatenAnts = o[QStringLiteral("eatenAnts")].toInt();
        sp.eatenSinceLastOffspring = o[QStringLiteral("eatenSinceLastOffspring")].toInt();
        sp.reproductionCooldown = o[QStringLiteral("reproductionCooldown")].toDouble();
        sp.dead = o[QStringLiteral("dead")].toBool();
        sp.ageSeconds = o[QStringLiteral("ageSeconds")].toDouble();
        sp.lifeTimeSeconds = o[QStringLiteral("lifeTimeSeconds")].toDouble(100.0);
        for (const QJsonValue& slot : o[QStringLiteral("digestionSlots")].toArray()) sp.digestionSlots.push_back(slot.toDouble());
        m_spiders.push_back(sp);
    }

    for (const QJsonValue& v : root[QStringLiteral("flowers")].toArray()) {
        const QJsonObject o = v.toObject();
        Flower fl;
        fl.pos = pointFromJson(o[QStringLiteral("pos")].toObject());
        fl.petalColor = colorFromJson(o[QStringLiteral("petalColor")].toObject(), randomFlowerColor());
        fl.spawnTimer = o[QStringLiteral("spawnTimer")].toDouble();
        fl.spawnInterval = o[QStringLiteral("spawnInterval")].toDouble(30.0);
        fl.foodPerSpawn = o[QStringLiteral("foodPerSpawn")].toInt(18);
        fl.maxNearbyFood = o[QStringLiteral("maxNearbyFood")].toInt(180);
        fl.scatterRadius = o[QStringLiteral("scatterRadius")].toDouble(38.0);
        fl.visualPhase = o[QStringLiteral("visualPhase")].toDouble();
        m_flowers.push_back(fl);
    }

    for (const QJsonValue& v : root[QStringLiteral("wasps")].toArray()) {
        const QJsonObject o = v.toObject();
        Wasp w;
        w.pos = pointFromJson(o[QStringLiteral("pos")].toObject());
        w.dir = pointFromJson(o[QStringLiteral("dir")].toObject(), QPointF(1.0, 0.0));
        w.dead = o[QStringLiteral("dead")].toBool();
        w.ageSeconds = o[QStringLiteral("ageSeconds")].toDouble();
        w.lifeTimeSeconds = o[QStringLiteral("lifeTimeSeconds")].toDouble(45.0);
        w.wanderTime = o[QStringLiteral("wanderTime")].toDouble();
        w.attackCooldown = o[QStringLiteral("attackCooldown")].toDouble();
        w.wingPhase = o[QStringLiteral("wingPhase")].toDouble();
        w.killedSpiders = o[QStringLiteral("killedSpiders")].toInt();
        m_wasps.push_back(w);
    }

    for (const QJsonValue& v : root[QStringLiteral("queens")].toArray()) {
        const QJsonObject o = v.toObject();
        Queen q;
        q.pos = pointFromJson(o[QStringLiteral("pos")].toObject());
        q.dir = pointFromJson(o[QStringLiteral("dir")].toObject(), QPointF(1.0, 0.0));
        q.color = colorFromJson(o[QStringLiteral("color")].toObject(), QColor(255, 0, 0));
        q.dead = o[QStringLiteral("dead")].toBool();
        q.wingPhase = o[QStringLiteral("wingPhase")].toDouble();
        q.wanderTime = o[QStringLiteral("wanderTime")].toDouble();
        q.flightTime = o[QStringLiteral("flightTime")].toDouble();
        q.explorationDurationSeconds =
            o[QStringLiteral("explorationDurationSeconds")].toDouble(45.0);
        q.surveyTimer = o[QStringLiteral("surveyTimer")].toDouble();
        q.hasBestFoodPosition = o[QStringLiteral("hasBestFoodPosition")].toBool();
        q.bestFoodPos = pointFromJson(o[QStringLiteral("bestFoodPos")].toObject(), q.pos);
        q.bestFoodAmount = o[QStringLiteral("bestFoodAmount")].toInt();
        q.returningToBest = o[QStringLiteral("returningToBest")].toBool();
        q.hasFoundingSite = o[QStringLiteral("hasFoundingSite")].toBool(
            q.returningToBest && q.hasBestFoodPosition
            );
        q.foundingSitePos = pointFromJson(
            o[QStringLiteral("foundingSitePos")].toObject(),
            q.bestFoodPos
            );
        m_queens.push_back(q);
    }

    for (const QJsonValue& v : root[QStringLiteral("spiderHoles")].toArray()) {
        const QJsonObject o = v.toObject();
        SpiderHole h;
        h.pos = pointFromJson(o[QStringLiteral("pos")].toObject());
        h.health = std::clamp(
            o[QStringLiteral("health")].toDouble(MaximumNestHealth),
            0.0,
            MaximumNestHealth
            );
        h.spawnTimer = o[QStringLiteral("spawnTimer")].toDouble();
        h.spawnInterval = o[QStringLiteral("spawnInterval")].toDouble(90.0);
        h.localRadius = o[QStringLiteral("localRadius")].toDouble(180.0);
        h.maxLocalSpiders = o[QStringLiteral("maxLocalSpiders")].toInt(5);
        h.visualPhase = o[QStringLiteral("visualPhase")].toDouble();
        m_spiderHoles.push_back(h);
    }

    for (const QJsonValue& v : root[QStringLiteral("dynamite")].toArray()) {
        const QJsonObject o = v.toObject();
        DynamiteCharge d;
        d.pos = pointFromJson(o[QStringLiteral("pos")].toObject());
        d.groundColor = colorFromJson(o[QStringLiteral("groundColor")].toObject(), QColor(225, 210, 160));
        d.ignited = o[QStringLiteral("ignited")].toBool();
        d.exploded = o[QStringLiteral("exploded")].toBool();
        d.fuseTime = o[QStringLiteral("fuseTime")].toDouble();
        d.fuseDuration = o[QStringLiteral("fuseDuration")].toDouble(1.15);
        d.visualPhase = o[QStringLiteral("visualPhase")].toDouble();
        m_dynamite.push_back(d);
    }

    removeDepletedFoodSources();
    updateColonyDeathStates();
    ++m_terrainRevision;
    return true;
}

void GameWorld::createFallback()
{
    m_map.createFallback(180, 110, 10);
    rebuildFromMap();
}

bool GameWorld::hasMap() const
{
    return m_map.isValid();
}

const WorldMap& GameWorld::map() const
{
    return m_map;
}

double GameWorld::worldTimeSeconds() const
{
    return m_worldTimeSeconds;
}

quint64 GameWorld::terrainRevision() const
{
    return m_terrainRevision;
}

int GameWorld::colonyCount() const
{
    return m_colonies.size();
}

int GameWorld::totalAntCount() const
{
    return m_ants.size();
}

QColor GameWorld::colonyColor(int colonyId) const
{
    if (colonyId < 0 || colonyId >= m_colonies.size()) {
        return QColor(255, 255, 255);
    }

    return m_colonies[colonyId].color;
}

bool GameWorld::setColonyColor(int colonyId, const QColor& color)
{
    if (colonyId < 0 || colonyId >= m_colonies.size() || !color.isValid()) {
        return false;
    }

    QColor opaqueColor = color;
    opaqueColor.setAlpha(255);

    if (m_colonies[colonyId].color == opaqueColor) {
        return false;
    }

    m_colonies[colonyId].color = opaqueColor;
    return true;
}

int GameWorld::antCountForColony(int colonyId) const
{
    if (colonyId < 0 || colonyId >= m_colonies.size()) {
        return 0;
    }

    return m_colonies[colonyId].antCount;
}

int GameWorld::colonyStoredFood(int colonyId) const
{
    if (colonyId < 0 || colonyId >= m_colonies.size()) {
        return 0;
    }

    return m_colonies[colonyId].storedFood;
}

int GameWorld::colonyScore(int colonyId) const
{
    if (colonyId < 0 || colonyId >= m_colonies.size()) {
        return 0;
    }

    const Colony& colony = m_colonies[colonyId];

    return colony.antCount + colony.storedFood * 3;
}

QPointF GameWorld::colonyHome(int colonyId) const
{
    if (colonyId < 0 || colonyId >= m_colonies.size()) {
        return QPointF();
    }

    return m_colonies[colonyId].home;
}

bool GameWorld::colonyIsDead(int colonyId) const
{
    if (colonyId < 0 || colonyId >= m_colonies.size()) {
        return true;
    }

    return m_colonies[colonyId].dead;
}

bool GameWorld::colonyIsDestroyed(int colonyId) const
{
    if (colonyId < 0 || colonyId >= m_colonies.size()) {
        return true;
    }

    return m_colonies[colonyId].destroyed;
}

double GameWorld::colonyNestHealth(int colonyId) const
{
    if (colonyId < 0 || colonyId >= m_colonies.size()) {
        return 0.0;
    }

    return m_colonies[colonyId].nestHealth;
}

double GameWorld::colonyDeathTimeSeconds(int colonyId) const
{
    if (colonyId < 0 || colonyId >= m_colonies.size()) {
        return -1.0;
    }

    return m_colonies[colonyId].deathTimeSeconds;
}


int GameWorld::spiderCount() const
{
    int count = 0;

    for (const Spider& spider : m_spiders) {
        if (!spider.dead) {
            ++count;
        }
    }

    return count;
}

int GameWorld::waspCount() const
{
    int count = 0;

    for (const Wasp& wasp : m_wasps) {
        if (!wasp.dead) {
            ++count;
        }
    }

    return count;
}

void GameWorld::killSpiderAsFood(Spider& spider)
{
    if (spider.dead) {
        return;
    }

    spider.dead = true;

    const int foodAmount =
        45 + std::min(120, spider.eatenAnts * 6);

    spawnFoodAt(spider.pos, foodAmount);
}

double GameWorld::spiderDuelStrength(const Spider& spider) const
{
    const int usedSlots = spiderUsedSlots(spider);

    const double digestionStrength =
        usedSlots * 4.0;

    const double experienceStrength =
        std::min(30, spider.eatenAnts) * 0.55;

    const double ageStrength =
        std::min(spider.ageSeconds / 60.0, 8.0);

    const double randomJitter =
        (QRandomGenerator::global()->generateDouble() - 0.5) * 3.0;

    return digestionStrength
           + experienceStrength
           + ageStrength
           + randomJitter;
}

void GameWorld::resolveSpiderDuels()
{
    constexpr double duelDistance = 15.0;
    constexpr double duelDistanceSq = duelDistance * duelDistance;

    // Wenn die Basiswerte sehr nah beieinander liegen,
    // sterben beide.
    constexpr double drawMargin = 3.0;

    for (int i = 0; i < m_spiders.size(); ++i) {
        Spider& a = m_spiders[i];

        if (a.dead) {
            continue;
        }

        for (int j = i + 1; j < m_spiders.size(); ++j) {
            Spider& b = m_spiders[j];

            if (a.ageSeconds < 8.0 || b.ageSeconds < 8.0) {
                continue;
            }

            if (b.dead) {
                continue;
            }

            if (distanceSquared(a.pos, b.pos) > duelDistanceSq) {
                continue;
            }

            const double strengthA = spiderDuelStrength(a);
            const double strengthB = spiderDuelStrength(b);

            const double diff = strengthA - strengthB;

            if (std::abs(diff) <= drawMargin) {
                // Unentschieden: beide sterben.
                killSpiderAsFood(a);
                killSpiderAsFood(b);
                break;
            }

            if (diff > 0.0) {
                // A gewinnt.
                killSpiderAsFood(b);
                m_soundEvents.push_back({ SoundEffect::SpiderWinsDuel, b.pos });

                // Gewinner ist kurz beschäftigt / satt-träge.
                a.attackCooldown = std::max(a.attackCooldown, 1.2);
                a.wanderTime = 1.5;
            } else {
                // B gewinnt.
                killSpiderAsFood(a);
                m_soundEvents.push_back({ SoundEffect::SpiderWinsDuel, a.pos });

                b.attackCooldown = std::max(b.attackCooldown, 1.2);
                b.wanderTime = 1.5;

                break;
            }
        }
    }
}


void GameWorld::tick(double dt)
{
    if (!hasMap()) {
        return;
    }

    m_worldTimeSeconds += dt;
    updateNestHealth(dt);

    for (Ant& ant : m_ants) {
        if (ant.dead) {
            continue;
        }

        ant.ageSeconds += dt;

        if (ant.ageSeconds >= ant.lifeTimeSeconds) {
            ant.dead = true;
            continue;
        }

        handleAnt(ant, dt);
    }

    QVector<QPointF> spiderBirths;

    for (Spider& spider : m_spiders) {
        if (spider.dead) {
            continue;
        }

        spider.ageSeconds += dt;
        spider.reproductionCooldown =
            std::max(0.0, spider.reproductionCooldown - dt);

        if (spider.ageSeconds >= spider.lifeTimeSeconds) {
            killSpiderAsFood(spider);
            continue;
        }

        handleSpider(spider, dt);

        if (spiderWantsToReproduce(spider) &&
            spiderCount() + spiderBirths.size() < MaximumSpiderCount) {
            const QPointF childPos =
                findNearbyWalkablePosition(spider.pos, 18.0, 80.0);

            spiderBirths.push_back(childPos);

            spider.eatenSinceLastOffspring = 0;
            spider.reproductionCooldown =
                80.0 + QRandomGenerator::global()->generateDouble() * 50.0;
        }
    }

    resolveSpiderDuels();

    for (const QPointF& pos : spiderBirths) {
        spawnSpiderAt(pos);
    }

    for (Flower& flower : m_flowers) {
        handleFlower(flower, dt);
    }

    for (int i = m_queens.size() - 1; i >= 0; --i) {
        const bool readyToFound = handleQueen(m_queens[i], dt);

        if (m_queens[i].dead) {
            m_queens.removeAt(i);
            continue;
        }

        if (readyToFound && foundColonyFromQueen(m_queens[i])) {
            m_queens.removeAt(i);
        }
    }

    for (Wasp& wasp : m_wasps) {
        if (wasp.dead) {
            continue;
        }

        handleWasp(wasp, dt);
    }

    for (SpiderHole& hole : m_spiderHoles) {
        handleSpiderHole(hole, dt);
    }

    handleDynamite(dt);

    removeDeadAnts();
    updateColonyDeathStates();
    updateColonyMaturity(dt);
    removeExpiredColonies();

    removeDeadSpiders();
    removeDeadWasps();
    removeDepletedFoodSources();


    tickEffects(dt);
    m_pheromones.evaporateAndDiffuse(dt, m_map);

    for (int colonyId = 0; colonyId < m_colonies.size(); ++colonyId) {
        Colony& colony = m_colonies[colonyId];

        if (colony.dead || colony.destroyed || colony.nestHealth <= 0.0) {
            continue;
        }

        /*
        int cycles = 0;
        while (colony.storedFood >= 8 && colony.antCount < colony.maxAntCount) {
            colony.storedFood -= 8;

            cycles++;
            for (int i = 0; i < 3 && colony.antCount < colony.maxAntCount; ++i) {
                spawnAnt(colonyId);
            }
        }
        */
        int cycles = 0;
        while (colony.storedFood >= 3 && colony.antCount < colony.maxAntCount && cycles++ < 2) {
            colony.storedFood -= 3;
            spawnAnt(colonyId);
        }
    }
}

void GameWorld::setWeatherSpeedFactor(double factor)
{
    m_weatherSpeedFactor = std::clamp(factor, 0.0, 2.0);
}

void GameWorld::tickEffects(double dt)
{
    const QVector<AbstractEffect*> effects = m_effects;

    for (AbstractEffect* effect : effects) {
        if (!effect) {
            continue;
        }

        if (!m_effects.contains(effect)) {
            continue;
        }

        effect->tick(dt);
    }
}

QVector<SoundEvent> GameWorld::takeSoundEvents()
{
    QVector<SoundEvent> events;
    events.swap(m_soundEvents);
    return events;
}

void GameWorld::drawEntities(QPainter& painter, const QRectF& cameraWorldRect) const
{
    m_pheromones.draw(painter, cameraWorldRect);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    drawFood(painter,cameraWorldRect);
    drawSpiderHoles(painter, cameraWorldRect);
    drawColonies(painter,cameraWorldRect);
    drawAnts(painter,cameraWorldRect);
    drawSpiders(painter,cameraWorldRect);
    drawFlowers(painter, cameraWorldRect);
    drawQueens(painter, cameraWorldRect);
    drawWasps(painter, cameraWorldRect);
    drawDynamite(painter, cameraWorldRect);
    drawEffects(painter,cameraWorldRect);
    drawHudText(painter,cameraWorldRect);
}

void GameWorld::burnAt(const QPointF& worldPos, double radiusPx)
{
    if (!hasMap()) {
        return;
    }

    igniteDynamiteNear(worldPos, radiusPx, 0.0);
    m_soundEvents.push_back({ SoundEffect::Burn, worldPos });

    registerEffect(new BurnEffect(worldPos, radiusPx));

    const double radiusSq = radiusPx * radiusPx;

    // Ameisen verbrennen
    for (Ant& ant : m_ants) {
        if (ant.dead) {
            continue;
        }

        if (distanceSquared(ant.pos, worldPos) <= radiusSq) {
            ant.dead = true;
        }
    }

    // Spinnen entfernen, ohne Futter zu erzeugen.
    // "Weggebrannt" bedeutet hier: komplett weg.
    for (int i = m_spiders.size() - 1; i >= 0; --i) {
        if (distanceSquared(m_spiders[i].pos, worldPos) <= radiusSq) {
            m_spiders.removeAt(i);
        }
    }

    // Nahrung entfernen
    for (FoodSource& food : m_food) {
        if (distanceSquared(food.pos, worldPos) <= radiusSq) {
            food.amount = 0;
        }
    }
    removeDepletedFoodSources();

    // Erst vorhandene Nahrung verbrennen, dann das von den getoeteten
    // Ameisen fallengelassene Futter erzeugen.
    removeDeadAnts();
    updateColonyDeathStates();

    // Pheromone löschen
    m_pheromones.clearCircle(worldPos, radiusPx);
}

void GameWorld::placeFoodAt(const QPointF& worldPos, int amount)
{
    if (!hasMap()) {
        return;
    }

    if (amount <= 0) {
        return;
    }

    if (!m_map.isWalkableWorld(worldPos)) {
        return;
    }

    spawnFoodAt(worldPos, amount);
    m_soundEvents.push_back({ SoundEffect::FoodPlaced, worldPos });
}

void GameWorld::registerEffect(AbstractEffect* effect)
{
    Q_ASSERT(effect);

    QObject::connect(
        effect,
        &QObject::destroyed,
        [this, effect]() {
            m_effects.removeAll(effect);
        }
        );

    m_effects.push_back(effect);
}

void GameWorld::placeFlowerAt(const QPointF& worldPos)
{
    if (!hasMap()) {
        return;
    }

    if (!m_map.isWalkableWorld(worldPos)) {
        return;
    }

    spawnFlowerAt(worldPos);
    m_soundEvents.push_back({ SoundEffect::FlowerPlaced, worldPos });
}

void GameWorld::placePheromoneAt(const QPointF& worldPos, double radiusPx, double amount)
{
    if (!hasMap()) {
        return;
    }

    if (!m_map.isWalkableWorld(worldPos)) {
        return;
    }

    // Food-Pheromon: suchende Ameisen folgen dieser Spur.
    m_pheromones.depositCircle(
        PheromoneKind::Food,
        worldPos,
        radiusPx,
        amount,
        m_map
        );
}

void GameWorld::placeGrassAt(const QPointF& worldPos, double radiusPx)
{
    if (!hasMap()) {
        return;
    }

    if (m_map.paintCircleTerrain(worldPos, radiusPx, TerrainType::Grass)) {
        ++m_terrainRevision;
        rebuildHomeFlowFields();
        m_soundEvents.push_back({ SoundEffect::TerrainPainted, worldPos });
    }
}

void GameWorld::placeRockAt(const QPointF& worldPos, double radiusPx)
{
    if (!hasMap()) {
        return;
    }

    const double radiusSq = radiusPx * radiusPx;

    for (Ant& ant : m_ants) {
        if (!ant.dead && distanceSquared(ant.pos, worldPos) <= radiusSq) {
            ant.dead = true;
        }
    }

    removeDeadAnts();
    updateColonyDeathStates();

    for (int i = m_spiders.size() - 1; i >= 0; --i) {
        if (distanceSquared(m_spiders[i].pos, worldPos) <= radiusSq) {
            m_spiders.removeAt(i);
        }
    }

    if (m_map.paintCircleTerrain(worldPos, radiusPx, TerrainType::Rock)) {
        ++m_terrainRevision;
        rebuildHomeFlowFields();
        m_soundEvents.push_back({ SoundEffect::TerrainPainted, worldPos });
    }
}

void GameWorld::placeDynamiteAt(const QPointF& worldPos)
{
    if (!hasMap()) {
        return;
    }

    const QSize worldSize = m_map.worldSizePx();

    if (worldPos.x() < 0.0 ||
        worldPos.y() < 0.0 ||
        worldPos.x() >= worldSize.width() ||
        worldPos.y() >= worldSize.height()) {
        return;
    }

    constexpr double minDistance = 14.0;
    const double minDistanceSq = minDistance * minDistance;

    for (const DynamiteCharge& charge : m_dynamite) {
        if (!charge.exploded && distanceSquared(charge.pos, worldPos) <= minDistanceSq) {
            return;
        }
    }

    DynamiteCharge charge;
    charge.pos = worldPos;
    charge.groundColor = explosionColorForTerrain(
        m_map.terrainTypeAtWorld(worldPos),
        m_map.renderColorAtWorld(worldPos)
        );
    charge.fuseDuration = 1.05 + QRandomGenerator::global()->generateDouble() * 0.25;
    charge.visualPhase = QRandomGenerator::global()->generateDouble() * Pi * 2.0;

    m_dynamite.push_back(charge);
    m_soundEvents.push_back({ SoundEffect::DynamitePlaced, worldPos });
}

void GameWorld::spawnFlowerAt(const QPointF& pos)
{
    Flower flower;
    flower.pos = pos;
    flower.petalColor = randomFlowerColor();
    flower.foodPerSpawn =
        10 + static_cast<int>(QRandomGenerator::global()->bounded(18));

    flower.startInitialSpawnInterval();

    flower.maxNearbyFood = 140;
    flower.scatterRadius = 36.0 + QRandomGenerator::global()->generateDouble() * 20.0;
    flower.visualPhase = QRandomGenerator::global()->generateDouble() * 6.28318530718;

    m_flowers.push_back(flower);
}

void GameWorld::handleFlower(Flower& flower, double dt)
{
    flower.spawnTimer -= dt;
    flower.visualPhase += dt * 1.8;

    if (flower.spawnTimer > 0.0) {
        return;
    }

    flower.restartSpawnInterval();

    const int nearbyFood = foodAmountNear(flower.pos, flower.scatterRadius * 1.5);

    if (nearbyFood < flower.maxNearbyFood) {
        const QPointF spawnPos =
            findNearbyWalkablePosition(flower.pos, 4.0, flower.scatterRadius);

        spawnFoodAt(spawnPos, flower.foodPerSpawn);
    }

    // Sehr seltenes Ereignis:
    // Eine Blume kann gelegentlich eine Wespe erzeugen.
    constexpr int maxWasps = 10;

    if (waspCount() < maxWasps) {
        const double chance =
            0.012; // 1.2% pro Blumen-Produktionsereignis

        if (QRandomGenerator::global()->generateDouble() < chance) {
            spawnWaspAt(flower.pos);
        }
    }
}

int GameWorld::foodAmountNear(const QPointF& pos, double radiusPx) const
{
    const double radiusSq = radiusPx * radiusPx;
    int amount = 0;

    for (const FoodSource& food : m_food) {
        if (food.amount <= 0) {
            continue;
        }

        if (distanceSquared(food.pos, pos) <= radiusSq) {
            amount += food.amount;
        }
    }

    return amount;
}

QColor GameWorld::randomFlowerColor() const
{
    const int hue = QRandomGenerator::global()->bounded(360);
    const int saturation = 120 + QRandomGenerator::global()->bounded(100);
    const int value = 190 + QRandomGenerator::global()->bounded(60);

    return QColor::fromHsv(hue, saturation, value);
}

void GameWorld::spawnWaspAt(const QPointF& pos)
{
    Wasp wasp;

    wasp.pos = pos;
    wasp.dir = randomDirection();

    wasp.dead = false;
    wasp.ageSeconds = 0.0;

    // Seltenes Ereignis, aber sichtbar genug.
    wasp.lifeTimeSeconds =
        35.0 + QRandomGenerator::global()->generateDouble() * 45.0;

    wasp.wanderTime =
        0.4 + QRandomGenerator::global()->generateDouble() * 1.6;

    wasp.attackCooldown = 0.0;
    wasp.wingPhase =
        QRandomGenerator::global()->generateDouble() * 6.28318530718;

    wasp.killedSpiders = 0;

    m_wasps.push_back(wasp);
}

void GameWorld::handleWasp(Wasp& wasp, double dt)
{
    wasp.ageSeconds += dt;
    wasp.attackCooldown = std::max(0.0, wasp.attackCooldown - dt);
    wasp.wanderTime -= dt;
    wasp.wingPhase += dt * 28.0;

    const bool lifetimeExpired =
        wasp.ageSeconds >= wasp.lifeTimeSeconds;

    if (lifetimeExpired && isReachableByAnt(wasp.pos)) {
        killWaspAsFood(wasp);
        return;
    }

    QPointF desiredDir = wasp.dir;
    int spiderIndex = -1;

    if (lifetimeExpired) {
        // Ueberalterte Wespen suchen erreichbares Terrain auf, bevor sie
        // sterben und zu Futter werden.
        double bestDistance = Infinity;

        for (int i = 0; i < m_colonies.size() && i < m_homeFlowFields.size(); ++i) {
            const QPointF target = m_colonies[i].home;

            if (!m_homeFlowFields[i].isReachableWorld(target)) {
                continue;
            }

            const double candidateDistance = distanceSquared(wasp.pos, target);

            if (candidateDistance < bestDistance) {
                bestDistance = candidateDistance;
                desiredDir = normalized(
                    QPointF(
                        target.x() - wasp.pos.x(),
                        target.y() - wasp.pos.y()
                        )
                    );
            }
        }
    } else {
        spiderIndex = nearestLiveSpiderIndex(wasp.pos, 120.0);

        if (spiderIndex >= 0) {
            const Spider& spider = m_spiders[spiderIndex];

            desiredDir = normalized(
                QPointF(
                    spider.pos.x() - wasp.pos.x(),
                    spider.pos.y() - wasp.pos.y()
                    )
                );
        } else {
            if (wasp.wanderTime <= 0.0) {
                desiredDir = blendDirections(
                    wasp.dir,
                    0.55,
                    randomDirection(),
                    0.45
                    );

                wasp.wanderTime =
                    0.5 + QRandomGenerator::global()->generateDouble() * 1.5;
            }
        }
    }

    wasp.dir = steerWasp(wasp, desiredDir);

    const double speed =
        (spiderIndex >= 0
             ? 135.0
             : 95.0)
        * m_weatherSpeedFactor;

    wasp.pos = QPointF(
        wasp.pos.x() + wasp.dir.x() * speed * dt,
        wasp.pos.y() + wasp.dir.y() * speed * dt
        );

    keepWaspInsideWorld(wasp);

    if (lifetimeExpired && isReachableByAnt(wasp.pos)) {
        killWaspAsFood(wasp);
        return;
    }

    // Nach Bewegung prüfen, ob sie eine Spinne überfliegt.
    if (wasp.attackCooldown <= 0.0) {
        const int killIndex = nearestLiveSpiderIndex(wasp.pos, 10.0);

        if (killIndex >= 0) {
            Spider& spider = m_spiders[killIndex];

            killSpiderAsFood(spider);
            m_soundEvents.push_back({ SoundEffect::WaspKillsSpider, spider.pos });

            wasp.killedSpiders += 1;
            wasp.attackCooldown = 0.65;

            // Nach dem Stich leicht wegdrehen.
            wasp.dir = blendDirections(
                wasp.dir,
                0.45,
                randomDirection(),
                0.55
                );
        }
    }
}

QPointF GameWorld::steerWasp(
    const Wasp& wasp,
    const QPointF& desiredDir
    ) const
{
    QPointF dir = blendDirections(
        wasp.dir,
        0.82,
        desiredDir,
        0.18
        );

    // Kleine natürliche Flug-Unruhe.
    const double jitter =
        (QRandomGenerator::global()->generateDouble() - 0.5) * 0.32;

    dir = rotated(dir, jitter);

    return normalized(dir);
}

void GameWorld::keepWaspInsideWorld(Wasp& wasp)
{
    const auto worldSize = m_map.worldSizePx();

    const double minX = 2.0;
    const double minY = 2.0;
    const double maxX = std::max(2.0, static_cast<double>(worldSize.width()) - 2.0);
    const double maxY = std::max(2.0, static_cast<double>(worldSize.height()) - 2.0);

    bool bounced = false;

    if (wasp.pos.x() < minX) {
        wasp.pos.rx() = minX;
        wasp.dir.rx() = std::abs(wasp.dir.x());
        bounced = true;
    }

    if (wasp.pos.x() > maxX) {
        wasp.pos.rx() = maxX;
        wasp.dir.rx() = -std::abs(wasp.dir.x());
        bounced = true;
    }

    if (wasp.pos.y() < minY) {
        wasp.pos.ry() = minY;
        wasp.dir.ry() = std::abs(wasp.dir.y());
        bounced = true;
    }

    if (wasp.pos.y() > maxY) {
        wasp.pos.ry() = maxY;
        wasp.dir.ry() = -std::abs(wasp.dir.y());
        bounced = true;
    }

    if (bounced) {
        wasp.dir = normalized(wasp.dir);
        wasp.wanderTime = 0.2;
    }
}

int GameWorld::nearestLiveSpiderIndex(const QPointF& pos, double radiusPx) const
{
    int bestIndex = -1;
    double bestDist = radiusPx * radiusPx;

    for (int i = 0; i < m_spiders.size(); ++i) {
        const Spider& spider = m_spiders[i];

        if (spider.dead) {
            continue;
        }

        const double d = distanceSquared(pos, spider.pos);

        if (d < bestDist) {
            bestDist = d;
            bestIndex = i;
        }
    }

    return bestIndex;
}

bool GameWorld::isReachableByAnt(const QPointF& pos) const
{
    if (!m_map.isWalkableWorld(pos)) {
        return false;
    }

    for (const FlowField& field : m_homeFlowFields) {
        if (field.isReachableWorld(pos)) {
            return true;
        }
    }

    return false;
}

void GameWorld::killWaspAsFood(Wasp& wasp)
{
    if (wasp.dead || !isReachableByAnt(wasp.pos)) {
        return;
    }

    wasp.dead = true;

    const int foodAmount =
        25 + std::min(80, wasp.killedSpiders * 18);

    spawnFoodAt(wasp.pos, foodAmount);
}

void GameWorld::removeDeadWasps()
{
    for (int i = m_wasps.size() - 1; i >= 0; --i) {
        if (m_wasps[i].dead) {
            m_wasps.removeAt(i);
        }
    }
}

void GameWorld::spawnQueenAt(const QPointF& pos, const QColor& color)
{
    Queen queen;
    queen.pos = pos;
    queen.dir = randomDirection();
    queen.color = color;
    queen.wingPhase =
        QRandomGenerator::global()->generateDouble() * Pi * 2.0;
    queen.wanderTime =
        0.4 + QRandomGenerator::global()->generateDouble() * 1.6;
    queen.explorationDurationSeconds =
        30.0 + QRandomGenerator::global()->generateDouble() * 60.0;
    queen.bestFoodPos = pos;
    queen.foundingSitePos = pos;

    m_queens.push_back(queen);
}

void GameWorld::updateColonyMaturity(double dt)
{
    for (Colony& colony : m_colonies) {
        if (colony.dead || colony.destroyed) {
            continue;
        }

        if (colony.antCount <= ColonyMaturityPopulation) {
            if (!colony.mature) {
                colony.highPopulationTimeSeconds = 0.0;
            }
            continue;
        }

        if (!colony.mature) {
            colony.highPopulationTimeSeconds += dt;

            if (colony.highPopulationTimeSeconds <
                ColonyMaturityDurationSeconds) {
                continue;
            }

            colony.highPopulationTimeSeconds =
                ColonyMaturityDurationSeconds;
            colony.mature = true;
            spawnQueenAt(colony.home, colony.color);
            m_soundEvents.push_back({ SoundEffect::QueenSpawned, colony.home });
            colony.queenSpawnCooldownSeconds =
                QueenSpawnIntervalMinSeconds +
                QRandomGenerator::global()->generateDouble() *
                    QueenSpawnIntervalRangeSeconds;
            continue;
        }

        colony.queenSpawnCooldownSeconds =
            std::max(0.0, colony.queenSpawnCooldownSeconds - dt);

        if (colony.queenSpawnCooldownSeconds > 0.0) {
            continue;
        }

        spawnQueenAt(colony.home, colony.color);
        m_soundEvents.push_back({ SoundEffect::QueenSpawned, colony.home });
        colony.queenSpawnCooldownSeconds =
            QueenSpawnIntervalMinSeconds +
            QRandomGenerator::global()->generateDouble() *
                QueenSpawnIntervalRangeSeconds;
    }
}

bool GameWorld::handleQueen(Queen& queen, double dt)
{
    constexpr double explorationSpeed = 92.0;
    constexpr double returnSpeed = 125.0;
    constexpr double arrivalRadius = 5.0;
    constexpr double surveyInterval = 0.20;

    if (queen.dead) {
        return false;
    }

    if (queen.hasBestFoodPosition &&
        queen.bestFoodAmount < QueenMinimumFoodForColony) {
        queen.hasBestFoodPosition = false;
        queen.hasFoundingSite = false;
        queen.returningToBest = false;
        queen.bestFoodAmount = 0;
    }

    if (queen.hasBestFoodPosition &&
        !isQueenFoundingSiteValid(queen.bestFoodPos)) {
        QPointF replacementSite;

        if (findQueenFoundingSite(queen.bestFoodPos, replacementSite)) {
            queen.bestFoodPos = replacementSite;

            if (queen.hasFoundingSite) {
                queen.foundingSitePos = replacementSite;
            }
        } else {
            queen.hasBestFoodPosition = false;
            queen.hasFoundingSite = false;
            queen.returningToBest = false;
            queen.bestFoodAmount = 0;
        }
    }

    queen.wingPhase += dt * 28.0;
    queen.flightTime += dt;

    if (queen.returningToBest) {
        if (!queen.hasBestFoodPosition) {
            queen.returningToBest = false;
            queen.hasFoundingSite = false;
            return false;
        }

        if (!queen.hasFoundingSite ||
            !isQueenFoundingSiteValid(queen.foundingSitePos)) {
            QPointF replacementSite;

            if (findQueenFoundingSite(queen.bestFoodPos, replacementSite)) {
                queen.hasFoundingSite = true;
                queen.foundingSitePos = replacementSite;
            } else {
                queen.returningToBest = false;
                queen.hasBestFoodPosition = false;
                queen.hasFoundingSite = false;
                queen.bestFoodAmount = 0;
                queen.surveyTimer = 0.0;
                queen.flightTime = 0.0;
            }
            return false;
        }

        const QPointF toTarget(
            queen.foundingSitePos.x() - queen.pos.x(),
            queen.foundingSitePos.y() - queen.pos.y()
            );
        const double targetDistance =
            std::sqrt(toTarget.x() * toTarget.x() + toTarget.y() * toTarget.y());

        if (targetDistance <= arrivalRadius) {
            queen.pos = queen.foundingSitePos;
            return true;
        }

        queen.dir = normalized(toTarget);
        const double step = std::min(targetDistance, returnSpeed * dt);
        queen.pos = QPointF(
            queen.pos.x() + queen.dir.x() * step,
            queen.pos.y() + queen.dir.y() * step
            );

        if (step >= targetDistance) {
            queen.pos = queen.foundingSitePos;
            return true;
        }

        return false;
    }

    queen.wanderTime -= dt;
    queen.surveyTimer -= dt;

    QPointF desiredDir = queen.dir;

    if (queen.wanderTime <= 0.0) {
        desiredDir = blendDirections(
            queen.dir,
            0.55,
            randomDirection(),
            0.45
            );

        queen.wanderTime =
            0.5 + QRandomGenerator::global()->generateDouble() * 1.5;
    }

    queen.dir = steerQueen(queen, desiredDir);
    queen.pos = QPointF(
        queen.pos.x() + queen.dir.x() * explorationSpeed * dt,
        queen.pos.y() + queen.dir.y() * explorationSpeed * dt
        );

    keepQueenInsideWorld(queen);

    if (queen.surveyTimer <= 0.0) {
        updateQueenFoodMemory(queen);
        queen.surveyTimer = surveyInterval;
    }

    if (queen.flightTime >= queen.explorationDurationSeconds) {
        if (!queen.hasBestFoodPosition) {
            queen.dead = true;
            return false;
        }

        QPointF foundingSite;

        if (findQueenFoundingSite(queen.bestFoodPos, foundingSite)) {
            queen.hasFoundingSite = true;
            queen.foundingSitePos = foundingSite;
            queen.returningToBest = true;
        } else {
            queen.hasBestFoodPosition = false;
            queen.hasFoundingSite = false;
            queen.bestFoodAmount = 0;
            queen.flightTime = 0.0;
        }
    }

    return false;
}

bool GameWorld::foundColonyFromQueen(const Queen& queen)
{
    if (!queen.hasFoundingSite ||
        !isQueenFoundingSiteValid(queen.foundingSitePos)) {
        return false;
    }

    Colony colony;
    colony.home = queen.foundingSitePos;
    colony.color = queen.color;

    const int colonyId = m_colonies.size();
    m_colonies.push_back(colony);
    rebuildHomeFlowFields();

    for (int i = 0; i < QueenFoundingAntCount; ++i) {
        spawnAnt(colonyId);
    }

    m_soundEvents.push_back({ SoundEffect::ColonyFounded, colony.home });

    return true;
}

bool GameWorld::findQueenFoundingSite(
    const QPointF& favoritePos,
    QPointF& sitePos
    ) const
{
    const double spacing = std::max(10.0, static_cast<double>(m_map.tileSize()));
    const double maxRadius = std::max(120.0, spacing * 12.0);

    auto tileCenterNear = [this](const QPointF& pos) {
        const int tileSize = std::max(1, m_map.tileSize());
        const int tx = static_cast<int>(std::floor(pos.x() / tileSize));
        const int ty = static_cast<int>(std::floor(pos.y() / tileSize));
        return m_map.tileCenterWorld(tx, ty);
    };

    const QPointF center = tileCenterNear(favoritePos);
    if (isQueenFoundingSiteValid(center)) {
        sitePos = center;
        return true;
    }

    for (double radius = spacing; radius <= maxRadius; radius += spacing) {
        const int samples = std::max(
            12,
            static_cast<int>(std::ceil(Pi * 2.0 * radius / spacing))
            );
        const double phase =
            (static_cast<int>(radius / spacing) % 2 == 0)
                ? 0.5
                : 0.0;

        for (int sample = 0; sample < samples; ++sample) {
            const double angle =
                (static_cast<double>(sample) + phase) * Pi * 2.0 /
                static_cast<double>(samples);
            const QPointF candidate = tileCenterNear(
                QPointF(
                    favoritePos.x() + std::cos(angle) * radius,
                    favoritePos.y() + std::sin(angle) * radius
                    )
                );

            if (isQueenFoundingSiteValid(candidate)) {
                sitePos = candidate;
                return true;
            }
        }
    }

    return false;
}

bool GameWorld::isQueenFoundingSiteValid(const QPointF& pos) const
{
    const auto isDryWalkableTerrain = [this](const QPointF& point) {
        if (!m_map.isWalkableWorld(point)) {
            return false;
        }

        const TerrainType terrain = m_map.terrainTypeAtWorld(point);
        return terrain != TerrainType::Water &&
               terrain != TerrainType::ShallowWater;
    };

    if (!isDryWalkableTerrain(pos)) {
        return false;
    }

    const double footprintRadius =
        std::max(10.0, static_cast<double>(m_map.tileSize()));
    constexpr int footprintSamples = 8;

    for (int i = 0; i < footprintSamples; ++i) {
        const double angle =
            static_cast<double>(i) * Pi * 2.0 /
            static_cast<double>(footprintSamples);
        const QPointF probe(
            pos.x() + std::cos(angle) * footprintRadius,
            pos.y() + std::sin(angle) * footprintRadius
            );

        if (!isDryWalkableTerrain(probe)) {
            return false;
        }
    }

    constexpr double flowerClearance = 26.0;
    for (const Flower& flower : m_flowers) {
        if (distanceSquared(pos, flower.pos) <
            flowerClearance * flowerClearance) {
            return false;
        }
    }

    constexpr double colonyClearance = 44.0;
    for (const Colony& colony : m_colonies) {
        if (distanceSquared(pos, colony.home) <
                colonyClearance * colonyClearance) {
            return false;
        }
    }

    constexpr double spiderHoleClearance = 28.0;
    for (const SpiderHole& hole : m_spiderHoles) {
        if (hole.health > 0.0 &&
            distanceSquared(pos, hole.pos) <
            spiderHoleClearance * spiderHoleClearance) {
            return false;
        }
    }

    constexpr double dynamiteClearance = 18.0;
    for (const DynamiteCharge& charge : m_dynamite) {
        if (!charge.exploded &&
            distanceSquared(pos, charge.pos) <
                dynamiteClearance * dynamiteClearance) {
            return false;
        }
    }

    return true;
}

QPointF GameWorld::steerQueen(
    const Queen& queen,
    const QPointF& desiredDir
    ) const
{
    QPointF dir = blendDirections(
        queen.dir,
        0.82,
        desiredDir,
        0.18
        );

    const double jitter =
        (QRandomGenerator::global()->generateDouble() - 0.5) * 0.32;

    return normalized(rotated(dir, jitter));
}

void GameWorld::keepQueenInsideWorld(Queen& queen)
{
    const QSize worldSize = m_map.worldSizePx();

    const double minX = 2.0;
    const double minY = 2.0;
    const double maxX = std::max(2.0, static_cast<double>(worldSize.width()) - 2.0);
    const double maxY = std::max(2.0, static_cast<double>(worldSize.height()) - 2.0);

    bool bounced = false;

    if (queen.pos.x() < minX) {
        queen.pos.rx() = minX;
        queen.dir.rx() = std::abs(queen.dir.x());
        bounced = true;
    } else if (queen.pos.x() > maxX) {
        queen.pos.rx() = maxX;
        queen.dir.rx() = -std::abs(queen.dir.x());
        bounced = true;
    }

    if (queen.pos.y() < minY) {
        queen.pos.ry() = minY;
        queen.dir.ry() = std::abs(queen.dir.y());
        bounced = true;
    } else if (queen.pos.y() > maxY) {
        queen.pos.ry() = maxY;
        queen.dir.ry() = -std::abs(queen.dir.y());
        bounced = true;
    }

    if (bounced) {
        queen.dir = normalized(queen.dir);
        queen.wanderTime = 0.2;
    }
}

void GameWorld::updateQueenFoodMemory(Queen& queen)
{
    constexpr double surveyRadius = 100.0;
    const double radiusSq = surveyRadius * surveyRadius;

    int observedFood = 0;

    for (const FoodSource& food : m_food) {
        if (food.amount <= 0 || distanceSquared(queen.pos, food.pos) > radiusSq) {
            continue;
        }

        observedFood += food.amount;
    }

    if (observedFood < QueenMinimumFoodForColony) {
        return;
    }

    if (queen.hasBestFoodPosition && observedFood <= queen.bestFoodAmount) {
        return;
    }

    QPointF foundingSite;
    if (!findQueenFoundingSite(queen.pos, foundingSite)) {
        return;
    }

    queen.hasBestFoodPosition = true;
    queen.bestFoodAmount = observedFood;
    queen.bestFoodPos = foundingSite;
}

void GameWorld::handleSpiderHole(SpiderHole& hole, double dt)
{
    hole.visualPhase += dt * 1.2;

    if (hole.health <= 0.0) {
        return;
    }

    hole.spawnTimer -= dt;

    if (hole.spawnTimer > 0.0) {
        return;
    }

    hole.spawnInterval =
        55.0 + QRandomGenerator::global()->generateDouble() * 70.0;

    hole.spawnTimer = hole.spawnInterval;

    if (spiderCount() >= MaximumSpiderCount) {
        return;
    }

    // Lokale Bremse: ein Loch soll sein Gebiet gefährlich machen,
    // aber nicht unbegrenzt stapeln.
    if (spiderCountNear(hole.pos, hole.localRadius) >= hole.maxLocalSpiders) {
        return;
    }

    const QPointF spawnPos =
        findNearbyWalkablePosition(hole.pos, 8.0, 48.0);

    spawnSpiderAt(spawnPos);
}

int GameWorld::spiderCountNear(const QPointF& pos, double radiusPx) const
{
    const double radiusSq = radiusPx * radiusPx;
    int count = 0;

    for (const Spider& spider : m_spiders) {
        if (spider.dead) {
            continue;
        }

        if (distanceSquared(spider.pos, pos) <= radiusSq) {
            ++count;
        }
    }

    return count;
}

void GameWorld::drawSpiderHoles(QPainter& painter, const QRectF& cameraWorldRect) const
{
    Q_UNUSED(cameraWorldRect);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const SpiderHole& hole : m_spiderHoles) {
        const QPointF pos = hole.pos;
        const bool destroyed = hole.health <= 0.0;

        const double pulse =
            destroyed ? 1.0 : 1.0 + std::sin(hole.visualPhase) * 0.05;

        // Dunkler Rand
        painter.setPen(QPen(destroyed ? QColor(45, 42, 38) : QColor(45, 25, 15), 1.8));
        painter.setBrush(destroyed ? QColor(75, 70, 64) : QColor(85, 48, 26));

        painter.drawEllipse(
            QRectF(
                pos.x() - 12.0 * pulse,
                pos.y() - 8.0 * pulse,
                24.0 * pulse,
                16.0 * pulse
                )
            );

        // Loch innen
        painter.setPen(Qt::NoPen);
        painter.setBrush(destroyed ? QColor(35, 33, 30) : QColor(18, 12, 10));

        painter.drawEllipse(
            QRectF(
                pos.x() - 8.0 * pulse,
                pos.y() - 5.0 * pulse,
                16.0 * pulse,
                10.0 * pulse
                )
            );

        // Kleiner Sand-/Erdwall
        painter.setBrush(
            destroyed ? QColor(95, 90, 82, 150) : QColor(135, 90, 50, 180)
            );

        painter.drawEllipse(
            QRectF(
                pos.x() - 13.0,
                pos.y() + 4.0,
                9.0,
                4.0
                )
            );

        painter.drawEllipse(
            QRectF(
                pos.x() + 4.0,
                pos.y() + 3.5,
                10.0,
                4.0
                )
            );

        if (destroyed) {
            painter.setPen(QPen(QColor(30, 28, 26, 210), 1.5));
            painter.drawLine(pos + QPointF(-7.0, -4.0), pos + QPointF(7.0, 4.0));
            painter.drawLine(pos + QPointF(6.0, -5.0), pos + QPointF(-6.0, 5.0));
        } else {
            // Dezente Warnbeine / Spuren
            painter.setPen(QPen(QColor(35, 25, 20, 120), 1.0));
            painter.drawLine(QPointF(pos.x() - 9.0, pos.y() - 2.0), QPointF(pos.x() - 15.0, pos.y() - 6.0));
            painter.drawLine(QPointF(pos.x() + 9.0, pos.y() - 2.0), QPointF(pos.x() + 15.0, pos.y() - 6.0));
            painter.drawLine(QPointF(pos.x() - 8.0, pos.y() + 3.0), QPointF(pos.x() - 15.0, pos.y() + 7.0));
            painter.drawLine(QPointF(pos.x() + 8.0, pos.y() + 3.0), QPointF(pos.x() + 15.0, pos.y() + 7.0));
        }

        drawNestHealthBar(painter, pos, hole.health, 11.0);
    }

    painter.restore();
}

void GameWorld::spawnSpiderHoleAt(const QPointF& pos)
{
    SpiderHole hole;
    hole.pos = pos;

    hole.spawnInterval =
        55.0 + QRandomGenerator::global()->generateDouble() * 70.0;

    hole.spawnTimer =
        QRandomGenerator::global()->generateDouble() * hole.spawnInterval;

    hole.localRadius =
        150.0 + QRandomGenerator::global()->generateDouble() * 80.0;

    hole.maxLocalSpiders =
        3 + static_cast<int>(QRandomGenerator::global()->bounded(4)); // 3..6

    hole.visualPhase =
        QRandomGenerator::global()->generateDouble() * 6.28318530718;

    m_spiderHoles.push_back(hole);
}

// -----------------------------------------------------------------------------
// Dynamite
// -----------------------------------------------------------------------------

void GameWorld::handleDynamite(double dt)
{
    QVector<int> exploding;

    for (int i = 0; i < m_dynamite.size(); ++i) {
        DynamiteCharge& charge = m_dynamite[i];

        if (charge.exploded) {
            continue;
        }

        charge.visualPhase += dt * (charge.ignited ? 18.0 : 2.0);

        if (!charge.ignited) {
            continue;
        }

        charge.fuseTime += dt;

        if (charge.fuseTime >= charge.fuseDuration) {
            exploding.push_back(i);
        }
    }

    for (int i = exploding.size() - 1; i >= 0; --i) {
        explodeDynamite(exploding[i]);
    }

    removeExplodedDynamite();
}

void GameWorld::igniteDynamiteNear(
    const QPointF& pos,
    double radiusPx,
    double delaySeconds
    )
{
    const double radiusSq = radiusPx * radiusPx;

    for (DynamiteCharge& charge : m_dynamite) {
        if (charge.exploded || charge.ignited) {
            continue;
        }

        if (distanceSquared(charge.pos, pos) > radiusSq) {
            continue;
        }

        charge.ignited = true;
        charge.fuseTime = -std::max(0.0, delaySeconds);
    }
}

void GameWorld::igniteDynamiteFromExplosion(
    const QPointF& pos,
    double radiusPx
    )
{
    if (radiusPx <= 0.0) {
        return;
    }

    constexpr double nearestFuseSeconds = 0.10;
    constexpr double furthestFuseSeconds = 0.95;
    const double radiusSq = radiusPx * radiusPx;

    for (DynamiteCharge& charge : m_dynamite) {
        if (charge.exploded) {
            continue;
        }

        const double distanceSq = distanceSquared(charge.pos, pos);
        if (distanceSq > radiusSq) {
            continue;
        }

        const double distanceFactor = std::clamp(
            std::sqrt(distanceSq) / radiusPx,
            0.0,
            1.0
            );
        const double remainingFuseSeconds =
            nearestFuseSeconds
            + (furthestFuseSeconds - nearestFuseSeconds) * distanceFactor;
        const double acceleratedFuseTime =
            charge.fuseDuration - remainingFuseSeconds;

        charge.ignited = true;
        charge.fuseTime = std::max(charge.fuseTime, acceleratedFuseTime);
    }
}

void GameWorld::explodeDynamite(int index)
{
    if (index < 0 || index >= m_dynamite.size()) {
        return;
    }

    DynamiteCharge& charge = m_dynamite[index];

    if (charge.exploded) {
        return;
    }

    charge.exploded = true;

    constexpr double explosionRadius = 70.0;
    constexpr double chainRadius = 115.0;

    registerEffect(new ExplosionEffect(charge.pos, explosionRadius, charge.groundColor));
    m_soundEvents.push_back({ SoundEffect::Explosion, charge.pos });
    destroyEverythingInCircle(charge.pos, explosionRadius);
    igniteDynamiteFromExplosion(charge.pos, chainRadius);
}

void GameWorld::destroyEverythingInCircle(const QPointF& pos, double radiusPx)
{
    const double radiusSq = radiusPx * radiusPx;

    for (Ant& ant : m_ants) {
        if (!ant.dead && distanceSquared(ant.pos, pos) <= radiusSq) {
            ant.dead = true;
        }
    }

    for (int i = m_spiders.size() - 1; i >= 0; --i) {
        if (distanceSquared(m_spiders[i].pos, pos) <= radiusSq) {
            m_spiders.removeAt(i);
        }
    }

    for (int i = m_wasps.size() - 1; i >= 0; --i) {
        if (distanceSquared(m_wasps[i].pos, pos) <= radiusSq) {
            m_wasps.removeAt(i);
        }
    }

    for (int i = m_flowers.size() - 1; i >= 0; --i) {
        if (distanceSquared(m_flowers[i].pos, pos) <= radiusSq) {
            m_flowers.removeAt(i);
        }
    }

    damageNestsAt(pos, radiusPx);

    for (FoodSource& food : m_food) {
        if (distanceSquared(food.pos, pos) <= radiusSq) {
            food.amount = 0;
        }
    }
    removeDepletedFoodSources();

    // Die Explosion entfernt vorhandene Nahrung, aber nicht das Futter,
    // das getoetete Ameisen in diesem Moment fallen lassen.
    removeDeadAnts();

    // Die Basiskarte wird unter der Explosion zu normalem Boden. Die Nester
    // selbst bleiben als Simulationsobjekte beziehungsweise Ruinen erhalten.
    const bool terrainChanged = m_map.clearCircleToGround(pos, radiusPx);

    if (terrainChanged) {
        ++m_terrainRevision;
        rebuildHomeFlowFields();
    }

    m_pheromones.clearCircle(pos, radiusPx);
    updateColonyDeathStates();
}

void GameWorld::damageNestsAt(const QPointF& pos, double radiusPx)
{
    for (Colony& colony : m_colonies) {
        if (colony.dead || colony.destroyed || colony.nestHealth <= 0.0) {
            continue;
        }

        const double damage = explosionDamageAtDistance(
            distance(pos, colony.home),
            ColonyNestRadius,
            radiusPx
            );
        if (damage <= 0.0) {
            continue;
        }

        colony.nestHealth = std::max(0.0, colony.nestHealth - damage);
        if (colony.nestHealth <= 0.0) {
            colony.nestHealth = 0.0;
            colony.destroyed = true;
        }
    }

    for (SpiderHole& hole : m_spiderHoles) {
        if (hole.health <= 0.0) {
            continue;
        }

        const double damage = explosionDamageAtDistance(
            distance(pos, hole.pos),
            SpiderHoleRadius,
            radiusPx
            );
        if (damage <= 0.0) {
            continue;
        }

        hole.health = std::max(0.0, hole.health - damage);
    }
}

void GameWorld::removeExplodedDynamite()
{
    for (int i = m_dynamite.size() - 1; i >= 0; --i) {
        if (m_dynamite[i].exploded) {
            m_dynamite.removeAt(i);
        }
    }
}

QColor GameWorld::explosionColorForTerrain(
    TerrainType type,
    const QColor& renderColor
    ) const
{
    switch (type) {
    case TerrainType::Water:
        return QColor(58, 112, 205);

    case TerrainType::ShallowWater:
        return QColor(112, 205, 238);

    case TerrainType::Rock:
        return QColor(118, 118, 116);

    case TerrainType::Grass:
        return QColor(78, 145, 67);

    case TerrainType::Ground:
    case TerrainType::ColonyStart:
    case TerrainType::Food:
    default:
        return renderColor.isValid() ? renderColor : QColor(225, 210, 160);
    }
}

void GameWorld::drawDynamite(QPainter& painter, const QRectF& cameraWorldRect) const
{
    Q_UNUSED(cameraWorldRect);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const DynamiteCharge& charge : m_dynamite) {
        if (charge.exploded) {
            continue;
        }

        const QPointF pos = charge.pos;
        const double wobble = charge.ignited
            ? std::sin(charge.visualPhase) * 1.2
            : std::sin(charge.visualPhase) * 0.25;

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 55));
        painter.drawEllipse(QRectF(pos.x() - 12.0, pos.y() + 7.0, 24.0, 6.0));

        QRectF body(pos.x() - 11.0, pos.y() - 7.0 + wobble, 22.0, 14.0);

        painter.setPen(QPen(QColor(90, 20, 18), 1.2));
        painter.setBrush(QColor(205, 38, 35));
        painter.drawRoundedRect(body, 3, 3);

        painter.setBrush(QColor(245, 82, 70));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(QRectF(body.left() + 3.0, body.top() + 3.0, 8.0, 3.0), 1.5, 1.5);

        QFont oldFont = painter.font();
        QFont font = oldFont;
        font.setBold(true);
        font.setPointSizeF(6.5);
        painter.setFont(font);
        painter.setPen(QColor(255, 245, 210));
        painter.drawText(body, Qt::AlignCenter, QStringLiteral("TNT"));
        painter.setFont(oldFont);

        const QPointF fuseBase(pos.x(), body.top());
        const QPointF fuseTip(pos.x() + 7.0, body.top() - 7.0);

        painter.setPen(QPen(QColor(235, 205, 90), 1.8, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(fuseBase, QPointF(pos.x(), body.top() - 5.0));
        painter.drawLine(QPointF(pos.x(), body.top() - 5.0), fuseTip);

        if (charge.ignited) {
            const double fuseProgress = std::clamp(charge.fuseTime / charge.fuseDuration, 0.0, 1.0);
            const QPointF spark(
                fuseTip.x() + std::sin(charge.visualPhase * 1.7) * 1.6,
                fuseTip.y() + fuseProgress * 6.0
                );

            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 225, 80));
            painter.drawEllipse(spark, 3.0, 3.0);

            painter.setBrush(QColor(255, 70, 25, 180));
            painter.drawEllipse(spark + QPointF(1.3, -0.8), 1.7, 1.7);
        }
    }

    painter.restore();
    painter.setPen(Qt::NoPen);
}

// -----------------------------------------------------------------------------
// World setup
// -----------------------------------------------------------------------------

void GameWorld::rebuildFromMap()
{
    ++m_terrainRevision;
    m_worldTimeSeconds = 0.0;
    m_soundEvents.clear();

    m_colonies.clear();
    m_ants.clear();
    m_food.clear();
    m_spiders.clear();
    m_flowers.clear();
    m_wasps.clear();
    m_queens.clear();
    m_spiderHoles.clear();
    m_dynamite.clear();

    m_homeFlowFields.clear();


    m_pheromones.reset(
        m_map.widthTiles(),
        m_map.heightTiles(),
        m_map.tileSize()
        );

    const QVector<QPoint>& starts = m_map.colonyStarts();
    const QVector<QColor>& startColors = m_map.colonyStartColors();

    if (starts.isEmpty() && m_map.queenStarts().isEmpty()) {
        Colony colony;
        colony.home = QPointF(80, m_map.worldSizePx().height() / 2.0);
        colony.color = QColor(255, 0, 0);
        m_colonies.push_back(colony);
    } else {
        for (int i = 0; i < starts.size(); ++i) {
            Colony colony;
            colony.home = m_map.tileCenterWorld(starts[i].x(), starts[i].y());

            if (i < startColors.size()) {
                colony.color = startColors[i];
            } else {
                const QColor fallbackColors[] = {
                    QColor(255, 0, 0),
                    QColor(255, 0, 255),
                    QColor(255, 128, 128)
                };

                colony.color = fallbackColors[i % 3];
            }

            m_colonies.push_back(colony);
        }
    }

    rebuildHomeFlowFields();

    const QVector<QPoint>& queenStarts = m_map.queenStarts();
    const QVector<QColor>& queenColors = m_map.queenStartColors();

    for (int i = 0; i < queenStarts.size(); ++i) {
        const QColor color =
            i < queenColors.size()
                ? queenColors[i]
                : QColor(255, 0, 0);

        spawnQueenAt(
            m_map.tileCenterWorld(queenStarts[i].x(), queenStarts[i].y()),
            color
            );
    }

    for (const QPoint& p : m_map.foodSpots()) {
        FoodSource source;
        source.pos = m_map.tileCenterWorld(p.x(), p.y());
        source.amount = 200;
        m_food.push_back(source);
    }

    for (const QPoint& p : m_map.spiderStarts()) {
        spawnSpiderAt(m_map.tileCenterWorld(p.x(), p.y()));
    }

    if (m_food.isEmpty()) {
        FoodSource source;
        source.pos = QPointF(
            m_map.worldSizePx().width() * 0.75,
            m_map.worldSizePx().height() * 0.5
            );
        source.amount = 200;
        m_food.push_back(source);
    }

    for (const QPoint& p : m_map.flowerStarts()) {
        spawnFlowerAt(m_map.tileCenterWorld(p.x(), p.y()));
    }

    for (const QPoint& p : m_map.spiderHoleStarts()) {
        spawnSpiderHoleAt(m_map.tileCenterWorld(p.x(), p.y()));
    }

    for (int colonyId = 0; colonyId < m_colonies.size(); ++colonyId) {
        for (int i = 0; i < 30; ++i) {
            spawnAnt(colonyId);
        }
    }
}

void GameWorld::rebuildHomeFlowFields()
{
    m_homeFlowFields.clear();
    m_homeFlowFields.resize(m_colonies.size());

    for (int i = 0; i < m_colonies.size(); ++i) {
        m_homeFlowFields[i].rebuild(m_map, m_colonies[i].home);
    }
}

void GameWorld::drawFood(QPainter &painter, const QRectF &cameraWorldRect) const
{
    for (const FoodSource& food : m_food) {
        if (food.amount <= 0) {
            continue;
        }

        const double radius = 4.0 + std::min(8, food.amount / 25);

        painter.setBrush(QColor(230, 190, 40));
        painter.drawEllipse(
            QRectF(
                food.pos.x() - radius,
                food.pos.y() - radius,
                radius * 2.0,
                radius * 2.0
                )
            );
    }

}

void GameWorld::drawColonies(QPainter &painter, const QRectF &cameraWorldRect) const
{
    Q_UNUSED(cameraWorldRect);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (int i = 0; i < m_colonies.size(); ++i) {
        const Colony& colony = m_colonies[i];

        const QPointF pos = colony.home;
        const bool dead = colony.dead || colony.antCount <= 0;
        const bool nestDestroyed = colony.destroyed || colony.nestHealth <= 0.0;
        const bool inactive = dead || nestDestroyed;

        // Schatten
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 45));
        painter.drawEllipse(
            QRectF(
                pos.x() - 17.0,
                pos.y() + 7.0,
                34.0,
                9.0
                )
            );

        // Erdwall außen
        QColor moundOuter = inactive
                                ? QColor(95, 88, 78, 120)
                                : QColor(145, 105, 62);

        painter.setPen(QPen(QColor(80, 55, 35), inactive ? 1.0 : 1.4));
        painter.setBrush(moundOuter);

        painter.drawEllipse(
            QRectF(
                pos.x() - 16.0,
                pos.y() - 10.0,
                32.0,
                24.0
                )
            );

        // Heller Sandbereich
        QColor moundInner = inactive
                                ? QColor(130, 120, 105, 90)
                                : QColor(190, 150, 90);

        painter.setPen(Qt::NoPen);
        painter.setBrush(moundInner);

        painter.drawEllipse(
            QRectF(
                pos.x() - 11.0,
                pos.y() - 6.0,
                22.0,
                15.0
                )
            );

        // Eingang / Loch
        painter.setBrush(inactive ? QColor(25, 25, 25, 150) : QColor(30, 22, 16));

        painter.drawEllipse(
            QRectF(
                pos.x() - 6.0,
                pos.y() - 1.5,
                12.0,
                8.0
                )
            );

        // Farbring / Koloniezuordnung
        QColor ringColor = colony.color;

        if (inactive) {
            ringColor.setAlpha(80);
        }

        painter.setPen(QPen(ringColor, inactive ? 2.0 : 2.8));
        painter.setBrush(Qt::NoBrush);

        painter.drawEllipse(
            QRectF(
                pos.x() - 17.5,
                pos.y() - 11.5,
                35.0,
                27.0
                )
            );

        // Kleine Sandkörner / Struktur
        painter.setPen(inactive ? QColor(70, 70, 70, 80) : QColor(105, 72, 42, 150));

        painter.drawPoint(QPointF(pos.x() - 10.0, pos.y() + 3.0));
        painter.drawPoint(QPointF(pos.x() + 9.0, pos.y() + 1.0));
        painter.drawPoint(QPointF(pos.x() - 4.0, pos.y() - 7.0));
        painter.drawPoint(QPointF(pos.x() + 3.0, pos.y() - 5.0));
        painter.drawPoint(QPointF(pos.x() + 12.0, pos.y() + 7.0));
        painter.drawPoint(QPointF(pos.x() - 13.0, pos.y() + 8.0));

        // Lebender Bau: kleiner farbiger Fähnchen-/Kristallmarker
        if (!inactive) {
            painter.setPen(QPen(QColor(35, 25, 18), 1.2));
            painter.drawLine(
                QPointF(pos.x(), pos.y() - 10.0),
                QPointF(pos.x(), pos.y() - 22.0)
                );

            QPolygonF flag;
            flag << QPointF(pos.x(), pos.y() - 22.0)
                 << QPointF(pos.x() + 10.0, pos.y() - 18.0)
                 << QPointF(pos.x(), pos.y() - 14.0);

            painter.setPen(QPen(QColor(40, 30, 30), 1.0));
            painter.setBrush(colony.color);
            painter.drawPolygon(flag);
        }

        // Toter Bau: Totenschädel
        if (dead) {
            QFont oldFont = painter.font();

            QFont skullFont = oldFont;
            skullFont.setPointSizeF(17.0);
            skullFont.setBold(true);

            painter.setFont(skullFont);
            painter.setPen(QColor(20, 20, 20));

            painter.drawText(
                QRectF(
                    pos.x() - 14.0,
                    pos.y() - 18.0,
                    28.0,
                    28.0
                    ),
                Qt::AlignCenter,
                QStringLiteral("☠")
                );

            painter.setFont(oldFont);
        }

        if (nestDestroyed && !dead) {
            painter.setPen(QPen(QColor(35, 30, 27, 220), 2.0));
            painter.drawLine(pos + QPointF(-9.0, -6.0), pos + QPointF(9.0, 7.0));
            painter.drawLine(pos + QPointF(8.0, -7.0), pos + QPointF(-8.0, 7.0));
        }

        drawNestHealthBar(painter, pos, colony.nestHealth, 18.0);
    }

    painter.restore();
    painter.setPen(Qt::NoPen);
}

void GameWorld::drawAnts(QPainter &painter, const QRectF &cameraWorldRect) const
{
    for (const Ant& ant : m_ants) {
        if (ant.colonyId < 0 || ant.colonyId >= m_colonies.size()) {
            continue;
        }

        const QColor color = m_colonies[ant.colonyId].color;

        painter.setBrush(color);
        painter.drawEllipse(
            QRectF(
                ant.pos.x() - 2.4,
                ant.pos.y() - 2.4,
                4.8,
                4.8
                )
            );

        if (ant.carryingFood) {
            painter.setBrush(QColor(250, 230, 70));
            painter.drawEllipse(
                QRectF(
                    ant.pos.x() - 1.2,
                    ant.pos.y() - 5.0,
                    2.4,
                    2.4
                    )
                );
        }

        if (ant.recovering) {
            painter.setBrush(QColor(255, 80, 80, 130));
            painter.drawEllipse(
                QRectF(
                    ant.pos.x() - 4.5,
                    ant.pos.y() - 4.5,
                    9.0,
                    9.0
                    )
                );
        }
    }

}

void GameWorld::drawSpiders(QPainter &painter, const QRectF &cameraWorldRect) const
{
    for (const Spider& spider : m_spiders) {

        if (spider.dead) {
            continue;
        }

        const QPointF forward = normalized(spider.dir);
        const QPointF side(-forward.y(), forward.x());

        painter.setPen(QPen(QColor(18, 16, 22), 1.4));

        for (int i = -2; i <= 2; ++i) {
            if (i == 0) {
                continue;
            }

            const double offset = i * 1.6;

            const QPointF base(
                spider.pos.x() + forward.x() * offset,
                spider.pos.y() + forward.y() * offset
                );

            const QPointF legA(
                base.x() + side.x() * 4.0,
                base.y() + side.y() * 4.0
                );

            const QPointF legB(
                base.x() + side.x() * 8.0 + forward.x() * (i > 0 ? 2.0 : -2.0),
                base.y() + side.y() * 8.0 + forward.y() * (i > 0 ? 2.0 : -2.0)
                );

            const QPointF legC(
                base.x() - side.x() * 4.0,
                base.y() - side.y() * 4.0
                );

            const QPointF legD(
                base.x() - side.x() * 8.0 + forward.x() * (i > 0 ? 2.0 : -2.0),
                base.y() - side.y() * 8.0 + forward.y() * (i > 0 ? 2.0 : -2.0)
                );

            painter.drawLine(legA, legB);
            painter.drawLine(legC, legD);
        }

        const int usedSlots = spiderUsedSlots(spider);
        const double bellyScale = 1.0 + usedSlots * 0.10;

        painter.setPen(Qt::NoPen);

        painter.setBrush(QColor(36, 31, 43));
        painter.drawEllipse(
            QRectF(
                spider.pos.x() - 5.5 * bellyScale,
                spider.pos.y() - 4.5 * bellyScale,
                11.0 * bellyScale,
                9.0 * bellyScale
                )
            );

        painter.setBrush(QColor(24, 22, 30));
        painter.drawEllipse(QRectF(spider.pos.x() - 3.5, spider.pos.y() - 7.0, 7.0, 6.0));

        painter.setBrush(QColor(150, 35, 35));
        painter.drawEllipse(QRectF(spider.pos.x() - 2.2, spider.pos.y() - 6.2, 1.4, 1.4));
        painter.drawEllipse(QRectF(spider.pos.x() + 0.8, spider.pos.y() - 6.2, 1.4, 1.4));
    }
}

void GameWorld::drawFlowers(QPainter& painter, const QRectF& cameraWorldRect) const
{
    Q_UNUSED(cameraWorldRect);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const Flower& flower : m_flowers) {
        const QPointF pos = flower.pos;

        painter.setPen(QPen(QColor(45, 105, 45), 1.6));
        painter.drawLine(
            QPointF(pos.x(), pos.y() + 5.0),
            QPointF(pos.x(), pos.y() - 4.0)
            );

        painter.setPen(Qt::NoPen);

        const double pulse =
            1.0 + std::sin(flower.visualPhase) * 0.08;

        const double petalRadius = 3.2 * pulse;
        const double centerRadius = 2.0;

        QColor petal = flower.petalColor;
        QColor petalDark = petal.darker(125);

        const QPointF offsets[] = {
            QPointF(0.0, -5.0),
            QPointF(4.5, -2.0),
            QPointF(3.0, 3.5),
            QPointF(-3.0, 3.5),
            QPointF(-4.5, -2.0)
        };

        painter.setBrush(petalDark);

        for (const QPointF& offset : offsets) {
            painter.drawEllipse(
                QPointF(pos.x() + offset.x(), pos.y() + offset.y()),
                petalRadius,
                petalRadius
                );
        }

        painter.setBrush(petal);

        for (const QPointF& offset : offsets) {
            painter.drawEllipse(
                QPointF(pos.x() + offset.x() * 0.92, pos.y() + offset.y() * 0.92),
                petalRadius * 0.78,
                petalRadius * 0.78
                );
        }

        painter.setBrush(QColor(240, 210, 60));
        painter.drawEllipse(pos, centerRadius, centerRadius);
    }

    painter.restore();
}

void GameWorld::drawQueens(QPainter& painter, const QRectF& cameraWorldRect) const
{
    Q_UNUSED(cameraWorldRect);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Jede Königin markiert ihren bisher besten Fundort mit einem Fähnchen
    // in ihrer Koloniefarbe. Bei einem besseren Fund wandert es automatisch.
    for (const Queen& queen : m_queens) {
        if (!queen.hasBestFoodPosition) {
            continue;
        }

        const QPointF pos = queen.bestFoodPos;
        const TerrainType terrain = m_map.terrainTypeAtWorld(pos);
        if (terrain == TerrainType::Water ||
            terrain == TerrainType::ShallowWater) {
            continue;
        }

        painter.setPen(QPen(QColor(35, 25, 18), 1.2));
        painter.drawLine(
            QPointF(pos.x(), pos.y() + 2.0),
            QPointF(pos.x(), pos.y() - 14.0)
            );

        QPolygonF flag;
        flag << QPointF(pos.x(), pos.y() - 14.0)
             << QPointF(pos.x() + 10.0, pos.y() - 10.0)
             << QPointF(pos.x(), pos.y() - 6.0);

        painter.setPen(QPen(QColor(40, 30, 30), 1.0));
        painter.setBrush(queen.color);
        painter.drawPolygon(flag);
    }

    for (const Queen& queen : m_queens) {
        const QPointF forward = normalized(queen.dir);
        const QPointF side(-forward.y(), forward.x());
        const QPointF pos = queen.pos;
        const double wingFlap =
            1.0 + std::sin(queen.wingPhase) * 0.22;

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 45));
        painter.drawEllipse(QRectF(pos.x() - 8.0, pos.y() + 7.0, 16.0, 5.0));

        painter.setPen(QPen(QColor(180, 210, 230, 130), 0.8));
        painter.setBrush(QColor(220, 240, 255, 120));

        const QPointF wingA(
            pos.x() - forward.x() * 1.5 + side.x() * 5.0,
            pos.y() - forward.y() * 1.5 + side.y() * 5.0
            );
        const QPointF wingB(
            pos.x() - forward.x() * 1.5 - side.x() * 5.0,
            pos.y() - forward.y() * 1.5 - side.y() * 5.0
            );

        painter.drawEllipse(QRectF(wingA.x() - 5.0, wingA.y() - 2.7 * wingFlap, 10.0, 5.4 * wingFlap));
        painter.drawEllipse(QRectF(wingB.x() - 5.0, wingB.y() - 2.7 * wingFlap, 10.0, 5.4 * wingFlap));

        painter.setPen(QPen(queen.color.darker(180), 1.0));
        painter.setBrush(queen.color);
        painter.drawEllipse(QRectF(pos.x() - 5.5, pos.y() - 3.8, 11.0, 7.6));

        const QPointF head(
            pos.x() + forward.x() * 6.0,
            pos.y() + forward.y() * 6.0
            );

        painter.setPen(Qt::NoPen);
        painter.setBrush(queen.color.darker(145));
        painter.drawEllipse(QRectF(head.x() - 3.0, head.y() - 3.0, 6.0, 6.0));

        QPolygonF crown;
        crown << QPointF(head.x() - 4.2, head.y() - 3.0)
              << QPointF(head.x() - 3.4, head.y() - 7.0)
              << QPointF(head.x() - 1.2, head.y() - 4.8)
              << QPointF(head.x(), head.y() - 8.0)
              << QPointF(head.x() + 1.4, head.y() - 4.8)
              << QPointF(head.x() + 3.6, head.y() - 7.0)
              << QPointF(head.x() + 4.2, head.y() - 3.0);

        painter.setPen(QPen(QColor(110, 75, 15), 0.9));
        painter.setBrush(QColor(250, 205, 45));
        painter.drawPolygon(crown);
    }

    painter.restore();
}

void GameWorld::drawWasps(QPainter& painter, const QRectF& cameraWorldRect) const
{
    Q_UNUSED(cameraWorldRect);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const Wasp& wasp : m_wasps) {
        if (wasp.dead) {
            continue;
        }

        const QPointF forward = normalized(wasp.dir);
        const QPointF side(-forward.y(), forward.x());

        const QPointF pos = wasp.pos;

        const double wingFlap =
            1.0 + std::sin(wasp.wingPhase) * 0.22;

        // Schatten
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 45));
        painter.drawEllipse(
            QRectF(
                pos.x() - 7.0,
                pos.y() + 6.0,
                14.0,
                5.0
                )
            );

        // Flügel
        QColor wingColor(220, 240, 255, 115);
        painter.setBrush(wingColor);
        painter.setPen(QPen(QColor(180, 210, 230, 130), 0.8));

        const QPointF wingCenterA(
            pos.x() - forward.x() * 1.5 + side.x() * 4.0,
            pos.y() - forward.y() * 1.5 + side.y() * 4.0
            );

        const QPointF wingCenterB(
            pos.x() - forward.x() * 1.5 - side.x() * 4.0,
            pos.y() - forward.y() * 1.5 - side.y() * 4.0
            );

        painter.drawEllipse(
            QRectF(
                wingCenterA.x() - 4.5,
                wingCenterA.y() - 2.5 * wingFlap,
                9.0,
                5.0 * wingFlap
                )
            );

        painter.drawEllipse(
            QRectF(
                wingCenterB.x() - 4.5,
                wingCenterB.y() - 2.5 * wingFlap,
                9.0,
                5.0 * wingFlap
                )
            );

        // Körper
        painter.setPen(QPen(QColor(45, 35, 20), 1.0));
        painter.setBrush(QColor(235, 195, 45));

        painter.drawEllipse(
            QRectF(
                pos.x() - 4.5,
                pos.y() - 3.0,
                9.0,
                6.0
                )
            );

        // Schwarze Streifen
        painter.setPen(QPen(QColor(35, 30, 20), 1.2));
        painter.drawLine(
            QPointF(pos.x() - side.x() * 2.0 - forward.x() * 1.5, pos.y() - side.y() * 2.0 - forward.y() * 1.5),
            QPointF(pos.x() + side.x() * 2.0 - forward.x() * 1.5, pos.y() + side.y() * 2.0 - forward.y() * 1.5)
            );

        painter.drawLine(
            QPointF(pos.x() - side.x() * 2.0 + forward.x() * 1.5, pos.y() - side.y() * 2.0 + forward.y() * 1.5),
            QPointF(pos.x() + side.x() * 2.0 + forward.x() * 1.5, pos.y() + side.y() * 2.0 + forward.y() * 1.5)
            );

        // Kopf
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(45, 35, 20));

        const QPointF head(
            pos.x() + forward.x() * 5.0,
            pos.y() + forward.y() * 5.0
            );

        painter.drawEllipse(
            QRectF(
                head.x() - 2.5,
                head.y() - 2.5,
                5.0,
                5.0
                )
            );

        // Stachel
        painter.setPen(QPen(QColor(35, 25, 15), 1.5));

        painter.drawLine(
            QPointF(pos.x() - forward.x() * 5.0, pos.y() - forward.y() * 5.0),
            QPointF(pos.x() - forward.x() * 9.0, pos.y() - forward.y() * 9.0)
            );
    }

    painter.restore();
}

void GameWorld::drawHudText(QPainter &painter, const QRectF &cameraWorldRect) const
{
    for (int i = 0; i < m_colonies.size(); ++i) {
        const Colony& colony = m_colonies[i];
        painter.setPen(Qt::black);
        painter.drawText(
            QPointF(colony.home.x() + 16, colony.home.y() - 8),
            QString::number(colony.storedFood)
            );
        painter.setPen(Qt::NoPen);
    }
}

void GameWorld::drawEffects(QPainter& painter, const QRectF& cameraWorldRect) const
{
    for (AbstractEffect* effect : m_effects) {
        if (effect) {
            effect->drawEffect(painter, cameraWorldRect);
        }
    }
}

void GameWorld::spawnAnt(int colonyId)
{
    if (colonyId < 0 || colonyId >= m_colonies.size()) {
        return;
    }

    if (m_colonies[colonyId].dead ||
        m_colonies[colonyId].destroyed ||
        m_colonies[colonyId].nestHealth <= 0.0) {
        return;
    }

    const QPointF home = m_colonies[colonyId].home;

    const double jx = (QRandomGenerator::global()->generateDouble() - 0.5) * 16.0;
    const double jy = (QRandomGenerator::global()->generateDouble() - 0.5) * 16.0;

    Ant ant;
    ant.colonyId = colonyId;
    m_colonies[colonyId].antCount++;
    ant.pos = QPointF(home.x() + jx, home.y() + jy);
    ant.dir = randomDirection();
    ant.carryingFood = false;

    ant.dead = false;
    ant.ageSeconds = 0.0;
    ant.lifeTimeSeconds =
        95.0 + QRandomGenerator::global()->generateDouble() * 85.0;

    ant.previousPos = ant.pos;
    ant.recoveryDir = ant.dir;

    resetNavigationMemory(ant);
    resetSearchMemory(ant);
    resetAntThinkSchedule(ant);

    m_ants.push_back(ant);
}

// -----------------------------------------------------------------------------
// Ant simulation
// -----------------------------------------------------------------------------

void GameWorld::handleAnt(Ant& ant, double dt)
{
    if (ant.colonyId < 0 || ant.colonyId >= m_colonies.size()) {
        return;
    }

    Colony& colony = m_colonies[ant.colonyId];

    const bool thinkNow = updateAntThinkTimer(ant, dt);

    if (ant.carryingFood) {
        handleCarryingAnt(ant, colony, dt, thinkNow);
    } else {
        handleSearchingAnt(ant, colony, dt, thinkNow);
    }
}

void GameWorld::handleCarryingAnt(
    Ant& ant,
    Colony& colony,
    double dt,
    bool thinkNow
    )
{
    // Billig genug: weiter jedes Tick.
    m_pheromones.deposit(PheromoneKind::Food, ant.pos, 120.0 * dt);

    if (distanceSquared(ant.pos, colony.home) < 10.0 * 10.0) {
        ant.carryingFood = false;
        colony.storedFood += 1;
        ant.dir = randomDirection();

        resetNavigationMemory(ant);
        resetSearchMemory(ant);
        resetAntThinkSchedule(ant);

        return;
    }

    // Recovery-Zustand ist relativ billig und wichtig.
    updateCarryingRecoveryState(ant, dt);

    QPointF desiredDir = homeDirectionForAnt(ant, colony);

    // Wenn keine teure Neuentscheidung nötig ist, nur weiterlaufen.
    // Ausnahme: Recovery läuft gerade, dann darf die Ameise häufiger korrigieren.
    if (!thinkNow && !ant.recovering) {
        moveAntCheap(ant, 45.0, dt, desiredDir);
        return;
    }

    if (ant.recovering) {
        ant.recoveryTime -= dt;

        if (ant.recoveryTime <= 0.0 || ant.stuckTime <= 0.15) {
            ant.recovering = false;
        } else {
            desiredDir = blendDirections(
                desiredDir,
                0.35,
                ant.recoveryDir,
                0.65
                );
        }
    }

    // Teure Entscheidung nur periodisch oder während Recovery.
    ant.dir = steerWithFeelers(
        ant,
        PheromoneKind::Home,
        desiredDir,
        6.0,
        0.25
        );

    const double baseSpeed = 45.0;
    const double terrainSpeed = m_map.speedAtWorld(ant.pos);
    const double step =
        baseSpeed * terrainSpeed * m_weatherSpeedFactor * dt;

    if (tryMoveAnt(ant, ant.dir, step)) {
        return;
    }

    beginRecovery(ant);

    ant.dir = steerWithFeelers(
        ant,
        PheromoneKind::Home,
        ant.recoveryDir,
        3.0,
        0.1
        );

    if (tryMoveAnt(ant, ant.dir, step)) {
        return;
    }

    const QPointF emergencyDir = avoidObstacleDirection(ant, desiredDir);
    ant.dir = emergencyDir;
    tryMoveAnt(ant, ant.dir, step);
}


void GameWorld::handleSearchingAnt(
    Ant& ant,
    Colony& colony,
    double dt,
    bool thinkNow
    )
{
    Q_UNUSED(colony);

    // Wichtig:
    // Kein resetNavigationMemory() und kein resetSearchMemory() hier.
    // Die Ameise braucht Such-Gedächtnis.

    m_pheromones.deposit(PheromoneKind::Home, ant.pos, 45.0 * dt);

    // Wichtig:
    // Wenn die Ameise ihr Ziel-Futter bereits fast erreicht hat,
    // soll sie es aufnehmen dürfen, auch wenn sie nicht exakt auf dem Punkt steht.
    // Das ist billig, weil nur der bereits gespeicherte Zielindex geprüft wird.
    if (ant.searchTargetFoodIndex >= 0) {
        if (tryCollectFoodAtIndex(ant, ant.searchTargetFoodIndex, 12.0)) {
            return;
        }
    }

    // Wenn nicht gedacht werden muss und keine Recovery aktiv ist:
    // einfach in aktueller Richtung weiterlaufen.
    if (!thinkNow && !ant.searchRecovering) {
        moveAntCheap(ant, 60.0, dt, ant.dir);
        return;
    }

    QPointF desiredDir = ant.dir;
    PheromoneKind followKind = PheromoneKind::Food;
    double desiredWeight = 1.2;
    double pheromoneWeight = 2.5;

    bool hasFoodTarget = false;
    bool closeFoodMode = false;

    QPointF foodTargetPos;
    int foodIndex = -1;
    double foodDistance = 0.0;

    // Teuer: nur bei Think oder während Recovery.
    foodIndex = nearestVisibleFoodIndex(ant.pos, 120.0);

    if (foodIndex >= 0) {
        FoodSource& food = m_food[foodIndex];

        hasFoodTarget = true;
        foodTargetPos = food.pos;

        if (tryCollectFoodAtIndex(ant, foodIndex, 18.0)) {
            return;
        }

        foodDistance = distance(ant.pos, food.pos);
        closeFoodMode = foodDistance <= 34.0;

        desiredDir = normalized(
            QPointF(
                food.pos.x() - ant.pos.x(),
                food.pos.y() - ant.pos.y()
                )
            );

        desiredWeight = 4.0;
        pheromoneWeight = 0.8;
    } else {
        if (QRandomGenerator::global()->generateDouble() < 0.025) {
            desiredDir = blendDirections(
                desiredDir,
                0.80,
                randomDirection(),
                0.20
                );
        }
    }

    updateSearchingRecoveryState(
        ant,
        hasFoodTarget,
        foodTargetPos,
        foodIndex,
        dt
        );

    if (closeFoodMode) {
        // Futter liegt sichtbar und sehr nah.
        // Jetzt soll die Ameise nicht wegen Hindernissen hinter dem Futter wegsteuern.
        ant.searchRecovering = false;
        ant.searchRecoveryTime = 0.0;

        desiredWeight = 8.0;
        pheromoneWeight = 0.0;
    }

    if (ant.searchRecovering) {
        ant.searchRecoveryTime -= dt;

        if (ant.searchRecoveryTime <= 0.0 || ant.searchStuckTime <= 0.10) {
            ant.searchRecovering = false;
        } else {
            desiredDir = blendDirections(
                desiredDir,
                0.25,
                ant.searchRecoveryDir,
                0.75
                );

            desiredWeight = 2.0;
            pheromoneWeight = 0.4;
        }
    }

    if (closeFoodMode) {
        ant.dir = steerToCloseFood(
            ant,
            desiredDir,
            foodDistance
            );
    } else {
        ant.dir = steerWithFeelers(
            ant,
            followKind,
            desiredDir,
            desiredWeight,
            pheromoneWeight
            );
    }

    const double baseSpeed = 60.0;
    const double terrainSpeed = m_map.speedAtWorld(ant.pos);
    const double step =
        baseSpeed * terrainSpeed * m_weatherSpeedFactor * dt;

    if (tryMoveAnt(ant, ant.dir, step)) {
        return;
    }
    if (closeFoodMode && foodIndex >= 0) {
        if (tryCollectFoodAtIndex(ant, foodIndex, 16.0)) {
            return;
        }
    }

    beginSearchRecovery(ant, desiredDir);

    ant.dir = steerWithFeelers(
        ant,
        followKind,
        ant.searchRecoveryDir,
        2.2,
        0.2
        );

    if (tryMoveAnt(ant, ant.dir, step)) {
        return;
    }

    ant.dir = avoidObstacleDirection(ant, desiredDir);
    tryMoveAnt(ant, ant.dir, step);
}

bool GameWorld::updateAntThinkTimer(Ant& ant, double dt)
{
    ant.thinkTimer -= dt;

    if (ant.thinkTimer > 0.0) {
        return false;
    }

    ant.thinkTimer += ant.thinkInterval;

    // Falls ein großer dt-Schritt kam, sauber auffangen.
    if (ant.thinkTimer <= 0.0) {
        ant.thinkTimer = ant.thinkInterval;
    }

    return true;
}

void GameWorld::moveAntCheap(
    Ant& ant,
    double baseSpeed,
    double dt,
    const QPointF& fallbackDir
    )
{
    const double terrainSpeed = m_map.speedAtWorld(ant.pos);
    const double step =
        baseSpeed * terrainSpeed * m_weatherSpeedFactor * dt;

    if (tryMoveAnt(ant, ant.dir, step)) {
        return;
    }

    // Wenn die aktuelle Richtung blockiert ist, soll die Ameise
    // sofort wieder denken dürfen.
    ant.thinkTimer = 0.0;

    ant.dir = avoidObstacleDirection(ant, fallbackDir);

    if (tryMoveAnt(ant, ant.dir, step)) {
        return;
    }

    // Letzter einfacher Ausweg.
    ant.dir = randomDirection();
    tryMoveAnt(ant, ant.dir, step);
}

void GameWorld::resetAntThinkSchedule(Ant& ant)
{
    // Leicht unterschiedliche Intervalle verhindern, dass alle Ameisen
    // im selben Frame denken.
    ant.thinkInterval =
        0.10 + QRandomGenerator::global()->generateDouble() * 0.12;

    ant.thinkTimer =
        QRandomGenerator::global()->generateDouble() * ant.thinkInterval;
}


// -----------------------------------------------------------------------------
// Carrying ant recovery
// -----------------------------------------------------------------------------

void GameWorld::updateCarryingRecoveryState(Ant& ant, double dt)
{
    const double movedDistance = distance(ant.pos, ant.previousPos);

    if (movedDistance < 0.25) {
        ant.stuckTime += dt;
    } else {
        ant.stuckTime = std::max(0.0, ant.stuckTime - dt * 3.0);
    }

    ant.previousPos = ant.pos;

    const double currentCost = homeCostForAnt(ant);

    if (!std::isfinite(currentCost)) {
        ant.noProgressTime += dt;
    } else {
        if (!std::isfinite(ant.previousHomeCost) || ant.previousHomeCost <= 0.0) {
            ant.previousHomeCost = currentCost;
        }

        const double progress = ant.previousHomeCost - currentCost;

        if (progress < 0.05) {
            ant.noProgressTime += dt;
        } else {
            ant.noProgressTime = std::max(0.0, ant.noProgressTime - dt * 2.0);
        }

        ant.previousHomeCost = currentCost;
    }

    if (!ant.recovering && (ant.stuckTime > 0.8 || ant.noProgressTime > 2.0)) {
        beginRecovery(ant);
    }
}

void GameWorld::beginRecovery(Ant& ant)
{
    ant.recovering = true;
    ant.recoveryTime =
        0.7 + QRandomGenerator::global()->generateDouble() * 1.4;

    ant.recoveryDir = chooseRecoveryDirection(ant);

    ant.stuckTime = 0.0;
    ant.noProgressTime = 0.0;
}

void GameWorld::resetNavigationMemory(Ant& ant)
{
    ant.previousPos = ant.pos;
    ant.stuckTime = 0.0;

    ant.previousHomeCost = Infinity;
    ant.noProgressTime = 0.0;

    ant.recovering = false;
    ant.recoveryTime = 0.0;
    ant.recoveryDir = ant.dir;
}

QPointF GameWorld::homeDirectionForAnt(const Ant& ant, const Colony& colony) const
{
    if (ant.colonyId >= 0 && ant.colonyId < m_homeFlowFields.size()) {
        const FlowField& field = m_homeFlowFields[ant.colonyId];

        const QPointF flowDir = field.directionAtWorld(ant.pos);

        if (flowDir.x() != 0.0 || flowDir.y() != 0.0) {
            return normalized(flowDir);
        }
    }

    return normalized(
        QPointF(
            colony.home.x() - ant.pos.x(),
            colony.home.y() - ant.pos.y()
            )
        );
}

double GameWorld::homeCostForAnt(const Ant& ant) const
{
    if (ant.colonyId < 0 || ant.colonyId >= m_homeFlowFields.size()) {
        return Infinity;
    }

    return m_homeFlowFields[ant.colonyId].costAtWorld(ant.pos);
}

// -----------------------------------------------------------------------------
// Steering
// -----------------------------------------------------------------------------

QPointF GameWorld::steerWithFeelers(
    const Ant& ant,
    PheromoneKind kind,
    const QPointF& desiredDir,
    double desiredWeight,
    double pheromoneWeight
    ) const
{
    const QPointF baseDir = normalized(
        blendDirections(ant.dir, 0.30, desiredDir, 0.70)
        );

    const double sensorDistance = 38.0;

    const double angles[] = {
        0.0,
        -0.25, 0.25,
        -0.55, 0.55,
        -0.90, 0.90,
        -1.25, 1.25,
        -1.70, 1.70,
        -2.25, 2.25
    };

    double bestScore = -1000000000.0;
    QPointF bestDir = baseDir;
    bool found = false;

    const double currentHomeCost = homeCostForAnt(ant);

    for (double angle : angles) {
        const QPointF dir = normalized(rotated(baseDir, angle));

        const QPointF nearProbe(
            ant.pos.x() + dir.x() * 8.0,
            ant.pos.y() + dir.y() * 8.0
            );

        const QPointF midProbe(
            ant.pos.x() + dir.x() * 20.0,
            ant.pos.y() + dir.y() * 20.0
            );

        const QPointF farProbe(
            ant.pos.x() + dir.x() * sensorDistance,
            ant.pos.y() + dir.y() * sensorDistance
            );

        if (!m_map.isWalkableWorld(nearProbe)) {
            continue;
        }

        found = true;

        double score = 0.0;

        if (m_map.isWalkableWorld(midProbe)) {
            score += 2.5;
        } else {
            score -= 5.0;
        }

        if (m_map.isWalkableWorld(farProbe)) {
            score += 2.0;
        } else {
            score -= 3.0;
        }

        score += dot(dir, normalized(desiredDir)) * desiredWeight;
        score += dot(dir, normalized(ant.dir)) * 0.9;

        const double pheromone = m_pheromones.sample(kind, farProbe);
        score += std::min(pheromone / 45.0, 3.0) * pheromoneWeight;

        // Für tragende Ameisen: Flowfield-Kosten aktiv verbessern.
        if (ant.carryingFood && std::isfinite(currentHomeCost)) {
            const double probeCost =
                ant.colonyId >= 0 && ant.colonyId < m_homeFlowFields.size()
                    ? m_homeFlowFields[ant.colonyId].costAtWorld(farProbe)
                    : Infinity;

            if (std::isfinite(probeCost)) {
                const double improvement = currentHomeCost - probeCost;
                score += std::clamp(improvement / 5.0, -3.0, 5.0);
            } else {
                score -= 8.0;
            }
        }

        score += (QRandomGenerator::global()->generateDouble() - 0.5) * 0.12;

        if (score > bestScore) {
            bestScore = score;
            bestDir = dir;
        }
    }

    if (!found) {
        return avoidObstacleDirection(ant, desiredDir);
    }

    return normalized(bestDir);
}

void GameWorld::resetSearchMemory(Ant& ant)
{
    ant.searchTargetFoodIndex = -1;

    ant.searchPreviousPos = ant.pos;
    ant.searchStuckTime = 0.0;

    ant.previousFoodDistance = 0.0;
    ant.foodNoProgressTime = 0.0;

    ant.searchRecovering = false;
    ant.searchRecoveryTime = 0.0;
    ant.searchRecoveryDir = ant.dir;
}

bool GameWorld::hasWalkableLine(
    const QPointF& from,
    const QPointF& to,
    double maxDistance,
    double stepPx
    ) const
{
    const QPointF delta(
        to.x() - from.x(),
        to.y() - from.y()
        );

    const double len =
        std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());

    if (len <= 0.001) {
        return true;
    }

    const double checkDistance = std::min(len, maxDistance);

    const QPointF dir(
        delta.x() / len,
        delta.y() / len
        );

    for (double d = stepPx; d <= checkDistance; d += stepPx) {
        const QPointF probe(
            from.x() + dir.x() * d,
            from.y() + dir.y() * d
            );

        if (!m_map.isWalkableWorld(probe)) {
            return false;
        }
    }

    return true;
}

int GameWorld::nearestVisibleFoodIndex(const QPointF& pos, double radiusPx) const
{
    int bestIndex = -1;
    double bestDist = radiusPx * radiusPx;

    for (int i = 0; i < m_food.size(); ++i) {
        if (m_food[i].amount <= 0) {
            continue;
        }

        const double d = distanceSquared(pos, m_food[i].pos);

        if (d >= bestDist) {
            continue;
        }

        if (!hasWalkableLine(pos, m_food[i].pos, radiusPx, 6.0)) {
            continue;
        }

        bestDist = d;
        bestIndex = i;
    }

    return bestIndex;
}

void GameWorld::updateSearchingRecoveryState(
    Ant& ant,
    bool hasFoodTarget,
    const QPointF& foodTargetPos,
    int foodTargetIndex,
    double dt
    )
{
    const double movedDistance = distance(ant.pos, ant.searchPreviousPos);

    if (movedDistance < 0.25) {
        ant.searchStuckTime += dt;
    } else {
        ant.searchStuckTime = std::max(0.0, ant.searchStuckTime - dt * 3.0);
    }

    ant.searchPreviousPos = ant.pos;

    if (!hasFoodTarget) {
        ant.searchTargetFoodIndex = -1;
        ant.previousFoodDistance = 0.0;
        ant.foodNoProgressTime = std::max(0.0, ant.foodNoProgressTime - dt * 2.0);

        if (!ant.searchRecovering && ant.searchStuckTime > 0.9) {
            beginSearchRecovery(ant, ant.dir);
        }

        return;
    }

    if (ant.searchTargetFoodIndex != foodTargetIndex) {
        ant.searchTargetFoodIndex = foodTargetIndex;
        ant.previousFoodDistance = distance(ant.pos, foodTargetPos);
        ant.foodNoProgressTime = 0.0;
        ant.searchRecovering = false;
        ant.searchRecoveryTime = 0.0;
        return;
    }

    const double currentDistance = distance(ant.pos, foodTargetPos);

    if (ant.previousFoodDistance <= 0.0) {
        ant.previousFoodDistance = currentDistance;
    }

    const double progress = ant.previousFoodDistance - currentDistance;

    if (progress < 0.12) {
        ant.foodNoProgressTime += dt;
    } else {
        ant.foodNoProgressTime = std::max(0.0, ant.foodNoProgressTime - dt * 2.5);
    }

    ant.previousFoodDistance = currentDistance;

    if (!ant.searchRecovering &&
        (ant.searchStuckTime > 0.65 || ant.foodNoProgressTime > 0.85)) {
        const QPointF desiredDir = normalized(
            QPointF(
                foodTargetPos.x() - ant.pos.x(),
                foodTargetPos.y() - ant.pos.y()
                )
            );

        beginSearchRecovery(ant, desiredDir);
    }
}

void GameWorld::beginSearchRecovery(Ant& ant, const QPointF& desiredDir)
{
    ant.searchRecovering = true;

    ant.searchRecoveryTime =
        0.65 + QRandomGenerator::global()->generateDouble() * 0.9;

    ant.searchRecoveryDir =
        chooseSearchRecoveryDirection(ant, desiredDir);

    ant.searchStuckTime = 0.0;
    ant.foodNoProgressTime = 0.0;
}

QPointF GameWorld::chooseSearchRecoveryDirection(
    const Ant& ant,
    const QPointF& desiredDir
    ) const
{
    double bestScore = -1000000000.0;
    QPointF bestDir = normalized(rotated(desiredDir, Pi));

    constexpr int samples = 28;

    for (int i = 0; i < samples; ++i) {
        const double angle =
            (static_cast<double>(i) / samples) * Pi * 2.0;

        const double jitter =
            (QRandomGenerator::global()->generateDouble() - 0.5) * 0.25;

        const QPointF dir =
            normalized(rotated(QPointF(1.0, 0.0), angle + jitter));

        if (!canMoveInDirection(ant.pos, dir, 6.0)) {
            continue;
        }

        double score = 0.0;

        if (canMoveInDirection(ant.pos, dir, 14.0)) {
            score += 2.0;
        }

        if (canMoveInDirection(ant.pos, dir, 30.0)) {
            score += 3.0;
        }

        if (canMoveInDirection(ant.pos, dir, 58.0)) {
            score += 2.2;
        }

        if (canMoveInDirection(ant.pos, dir, 90.0)) {
            score += 0.8;
        }

        // Nicht zu stark zum Ziel ziehen, sonst kommt das alte Problem zurück.
        score += dot(dir, normalized(desiredDir)) * 0.55;

        // Etwas Trägheit, damit es nicht hektisch flackert.
        score += dot(dir, normalized(ant.dir)) * 0.75;

        const QPointF pheromoneProbe(
            ant.pos.x() + dir.x() * 42.0,
            ant.pos.y() + dir.y() * 42.0
            );

        const double foodSignal =
            m_pheromones.sample(PheromoneKind::Food, pheromoneProbe);

        score += std::min(foodSignal / 70.0, 1.5);

        // Zufall ist hier Absicht: gleiche Situationen sollen nicht alle Ameisen gleich lösen.
        score += QRandomGenerator::global()->generateDouble() * 1.25;

        if (score > bestScore) {
            bestScore = score;
            bestDir = dir;
        }
    }

    return normalized(bestDir);
}

bool GameWorld::tryCollectFoodAtIndex(
    Ant& ant,
    int foodIndex,
    double radiusPx
    )
{
    if (foodIndex < 0 || foodIndex >= m_food.size()) {
        return false;
    }

    FoodSource& food = m_food[foodIndex];

    if (food.amount <= 0) {
        return false;
    }

    const double radiusSq = radiusPx * radiusPx;

    if (distanceSquared(ant.pos, food.pos) > radiusSq) {
        return false;
    }

    // Nicht durch Wände / Felsen / tiefes Wasser hindurch einsammeln.
    // Aber nahe Kanten sind okay, solange eine direkte freie Linie besteht.
    if (!hasWalkableLine(ant.pos, food.pos, radiusPx + 4.0, 4.0)) {
        return false;
    }

    food.amount -= 1;
    ant.carryingFood = true;

    resetSearchMemory(ant);
    resetNavigationMemory(ant);
    resetAntThinkSchedule(ant);

    if (ant.colonyId >= 0 && ant.colonyId < m_colonies.size()) {
        ant.dir = homeDirectionForAnt(ant, m_colonies[ant.colonyId]);
    } else {
        ant.dir = randomDirection();
    }

    return true;
}

QPointF GameWorld::steerToCloseFood(
    const Ant& ant,
    const QPointF& desiredDir,
    double foodDistance
    ) const
{
    const QPointF baseDir = normalized(
        blendDirections(ant.dir, 0.15, desiredDir, 0.85)
        );

    const double probeDistance =
        std::clamp(foodDistance - 2.0, 5.0, 18.0);

    const double angles[] = {
        0.0,
        -0.18, 0.18,
        -0.38, 0.38,
        -0.65, 0.65,
        -0.95, 0.95,
        -1.35, 1.35
    };

    double bestScore = -1000000000.0;
    QPointF bestDir = baseDir;
    bool found = false;

    for (double angle : angles) {
        const QPointF dir = normalized(rotated(baseDir, angle));

        // Nur bis kurz vor das Futter prüfen, nicht weit dahinter.
        if (!canMoveInDirection(ant.pos, dir, std::min(5.0, probeDistance))) {
            continue;
        }

        found = true;

        double score = 0.0;

        if (canMoveInDirection(ant.pos, dir, probeDistance)) {
            score += 5.0;
        } else {
            score -= 3.0;
        }

        // Futter hat jetzt klar Vorrang.
        score += dot(dir, normalized(desiredDir)) * 8.0;

        // Etwas Trägheit, damit die Ameise nicht zittert.
        score += dot(dir, normalized(ant.dir)) * 0.7;

        // Kleine Variation, damit nicht alle identisch reagieren.
        score += (QRandomGenerator::global()->generateDouble() - 0.5) * 0.15;

        if (score > bestScore) {
            bestScore = score;
            bestDir = dir;
        }
    }

    if (!found) {
        return normalized(desiredDir);
    }

    return normalized(bestDir);
}

QPointF GameWorld::chooseRecoveryDirection(const Ant& ant) const
{
    QPointF desiredDir = ant.dir;

    if (ant.carryingFood && ant.colonyId >= 0 && ant.colonyId < m_colonies.size()) {
        desiredDir = homeDirectionForAnt(ant, m_colonies[ant.colonyId]);
    }

    const double currentCost = homeCostForAnt(ant);

    double bestScore = -1000000000.0;
    QPointF bestDir = normalized(rotated(desiredDir, Pi));

    // Viele Richtungen scannen, absichtlich mit etwas Zufall.
    constexpr int samples = 32;

    for (int i = 0; i < samples; ++i) {
        const double baseAngle =
            (static_cast<double>(i) / samples) * Pi * 2.0;

        const double jitter =
            (QRandomGenerator::global()->generateDouble() - 0.5) * 0.35;

        const QPointF dir = normalized(rotated(QPointF(1.0, 0.0), baseAngle + jitter));

        if (!canMoveInDirection(ant.pos, dir, 6.0)) {
            continue;
        }

        double score = 0.0;

        if (canMoveInDirection(ant.pos, dir, 14.0)) {
            score += 2.0;
        }

        if (canMoveInDirection(ant.pos, dir, 32.0)) {
            score += 3.0;
        }

        if (canMoveInDirection(ant.pos, dir, 64.0)) {
            score += 2.0;
        }

        score += dot(dir, normalized(ant.dir)) * 0.4;
        score += dot(dir, normalized(desiredDir)) * 0.8;

        if (ant.carryingFood && std::isfinite(currentCost) && ant.colonyId >= 0 && ant.colonyId < m_homeFlowFields.size()) {
            const QPointF probe(
                ant.pos.x() + dir.x() * 48.0,
                ant.pos.y() + dir.y() * 48.0
                );

            const double probeCost =
                m_homeFlowFields[ant.colonyId].costAtWorld(probe);

            if (std::isfinite(probeCost)) {
                const double improvement = currentCost - probeCost;
                score += std::clamp(improvement / 4.0, -2.0, 6.0);
            } else {
                score -= 5.0;
            }
        }

        score += QRandomGenerator::global()->generateDouble() * 1.5;

        if (score > bestScore) {
            bestScore = score;
            bestDir = dir;
        }
    }

    return normalized(bestDir);
}

QPointF GameWorld::avoidObstacleDirection(const Ant& ant, const QPointF& desiredDir) const
{
    const QPointF baseDir = normalized(
        blendDirections(ant.dir, 0.35, desiredDir, 0.65)
        );

    const double angles[] = {
        -0.25, 0.25,
        -0.55, 0.55,
        -0.90, 0.90,
        -1.25, 1.25,
        -1.70, 1.70,
        -2.20, 2.20,
        3.14
    };

    double bestScore = -1000000000.0;
    QPointF bestDir = normalized(rotated(baseDir, Pi));
    bool found = false;

    for (double angle : angles) {
        const QPointF dir = normalized(rotated(baseDir, angle));

        if (!canMoveInDirection(ant.pos, dir, 5.0)) {
            continue;
        }

        found = true;

        double score = 0.0;

        if (canMoveInDirection(ant.pos, dir, 14.0)) {
            score += 2.0;
        }

        if (canMoveInDirection(ant.pos, dir, 28.0)) {
            score += 3.0;
        }

        if (canMoveInDirection(ant.pos, dir, 52.0)) {
            score += 2.0;
        }

        score += dot(dir, normalized(desiredDir)) * 1.2;
        score += dot(dir, normalized(ant.dir)) * 0.8;

        score += (QRandomGenerator::global()->generateDouble() - 0.5) * 0.2;

        if (score > bestScore) {
            bestScore = score;
            bestDir = dir;
        }
    }

    if (!found) {
        return normalized(rotated(baseDir, Pi));
    }

    return normalized(bestDir);
}

bool GameWorld::tryMoveAnt(Ant& ant, const QPointF& dir, double step) const
{
    if (step <= 0.0) {
        return false;
    }

    const QPointF next(
        ant.pos.x() + dir.x() * step,
        ant.pos.y() + dir.y() * step
        );

    if (!m_map.isWalkableWorld(next)) {
        return false;
    }

    ant.pos = next;
    return true;
}

bool GameWorld::canMoveInDirection(const QPointF& pos, const QPointF& dir, double distance) const
{
    const QPointF probe(
        pos.x() + dir.x() * distance,
        pos.y() + dir.y() * distance
        );

    return m_map.isWalkableWorld(probe);
}

// -----------------------------------------------------------------------------
// Queries / math helpers
// -----------------------------------------------------------------------------

int GameWorld::nearestFoodIndex(const QPointF& pos, double radiusPx) const
{
    int bestIndex = -1;
    double bestDist = radiusPx * radiusPx;

    for (int i = 0; i < m_food.size(); ++i) {
        if (m_food[i].amount <= 0) {
            continue;
        }

        const double d = distanceSquared(pos, m_food[i].pos);

        if (d < bestDist) {
            bestDist = d;
            bestIndex = i;
        }
    }

    return bestIndex;
}

QPointF GameWorld::randomDirection() const
{
    const double angle =
        QRandomGenerator::global()->generateDouble() * Pi * 2.0;

    return QPointF(
        std::cos(angle),
        std::sin(angle)
        );
}

QPointF GameWorld::normalized(const QPointF& v) const
{
    const double len =
        std::sqrt(v.x() * v.x() + v.y() * v.y());

    if (len <= 0.0001) {
        return randomDirection();
    }

    return QPointF(
        v.x() / len,
        v.y() / len
        );
}

QPointF GameWorld::blendDirections(
    const QPointF& a,
    double weightA,
    const QPointF& b,
    double weightB
    ) const
{
    return normalized(
        QPointF(
            a.x() * weightA + b.x() * weightB,
            a.y() * weightA + b.y() * weightB
            )
        );
}

QPointF GameWorld::rotated(const QPointF& v, double radians) const
{
    const double c = std::cos(radians);
    const double s = std::sin(radians);

    return QPointF(
        v.x() * c - v.y() * s,
        v.x() * s + v.y() * c
        );
}

double GameWorld::dot(const QPointF& a, const QPointF& b) const
{
    return a.x() * b.x() + a.y() * b.y();
}

double GameWorld::distanceSquared(const QPointF& a, const QPointF& b) const
{
    const double dx = a.x() - b.x();
    const double dy = a.y() - b.y();

    return dx * dx + dy * dy;
}

double GameWorld::distance(const QPointF& a, const QPointF& b) const
{
    return std::sqrt(distanceSquared(a, b));
}


void GameWorld::spawnSpiderAt(const QPointF& pos)
{
    Spider spider;
    spider.pos = pos;
    spider.dir = randomDirection();

    spider.wanderTime =
        0.5 + QRandomGenerator::global()->generateDouble() * 2.0;

    spider.attackCooldown = 0.0;

    spider.eatenAnts = 0;
    spider.eatenSinceLastOffspring = 0;
    spider.reproductionCooldown = 45.0;

    spider.dead = false;
    spider.ageSeconds = 0.0;

    // 7 bis 11 virtuelle Minuten Lebenszeit.
    spider.lifeTimeSeconds =
        // 420.0 + QRandomGenerator::global()->generateDouble() * 240.0;
        150.0 + QRandomGenerator::global()->generateDouble() * 150.0;

    // 5 Magenplätze. 0.0 bedeutet: frei.
    spider.digestionSlots = QVector<double>(5, 0.0);

    m_spiders.push_back(spider);
}


void GameWorld::handleSpider(Spider& spider, double dt)
{
    updateSpiderDigestion(spider, dt);

    spider.attackCooldown = std::max(0.0, spider.attackCooldown - dt);
    spider.wanderTime -= dt;

    QPointF desiredDir = spider.dir;

    const bool hungry = spiderIsHungry(spider);
    int targetIndex = -1;

    if (hungry) {
        targetIndex = nearestLiveAntIndex(spider.pos, 130.0);
    }

    if (hungry && targetIndex >= 0) {
        Ant& target = m_ants[targetIndex];

        const double eatDistance = 7.0;

        if (spider.attackCooldown <= 0.0 &&
            distanceSquared(spider.pos, target.pos) < eatDistance * eatDistance) {
            if (spiderEatAnt(spider, target)) {
                spider.attackCooldown = SpiderEatCooldownSeconds;
                spider.dir = randomDirection();
                return;
            }
        }

        desiredDir = normalized(
            QPointF(
                target.pos.x() - spider.pos.x(),
                target.pos.y() - spider.pos.y()
                )
            );
    } else {
        // Satt oder kein Ziel:
        // frei herumwandern.
        if (spider.wanderTime <= 0.0) {
            desiredDir = blendDirections(
                spider.dir,
                0.45,
                randomDirection(),
                0.55
                );

            spider.wanderTime =
                0.8 + QRandomGenerator::global()->generateDouble() * 2.4;
        }
    }

    spider.dir = steerSpider(spider, desiredDir);

    const double baseSpeed = hungry && targetIndex >= 0
                                 ? 38.0
                                 : 24.0;

    const double fullnessSpeedFactor = spiderSpeedFactor(spider);
    const double terrainSpeed = m_map.speedAtWorld(spider.pos);

    const double step =
        baseSpeed
        * fullnessSpeedFactor
        * terrainSpeed
        * m_weatherSpeedFactor
        * dt;

    if (tryMoveSpider(spider, spider.dir, step)) {
        return;
    }

    spider.dir = randomDirection();
    tryMoveSpider(spider, spider.dir, step);
}

void GameWorld::removeDeadAnts()
{
    for (int i = m_ants.size() - 1; i >= 0; --i) {
        if (m_ants[i].dead || m_ants[i].ageSeconds >= m_ants[i].lifeTimeSeconds) {
            const int colonyId = m_ants[i].colonyId;
            const QPointF deathPos = m_ants[i].pos;
            const bool dropsFood = m_ants[i].carryingFood;

            if (colonyId >= 0 && colonyId < m_colonies.size()) {
                m_colonies[colonyId].antCount = std::max(0, m_colonies[colonyId].antCount - 1);
            }

            m_ants.removeAt(i);

            if (dropsFood) {
                spawnFoodAt(deathPos, 1);
            }
        }
    }
}

void GameWorld::updateSpiderDigestion(Spider& spider, double dt)
{
    for (double& slot : spider.digestionSlots) {
        if (slot > 0.0) {
            slot = std::max(0.0, slot - dt);
        }
    }
}

bool GameWorld::spiderIsHungry(const Spider& spider) const
{
    for (double slot : spider.digestionSlots) {
        if (slot <= 0.0) {
            return true;
        }
    }

    return false;
}

bool GameWorld::spiderEatAnt(Spider& spider, Ant& ant)
{
    for (double& slot : spider.digestionSlots) {
        if (slot <= 0.0) {
            QColor antColor(220, 220, 220);

            if (ant.colonyId >= 0 && ant.colonyId < m_colonies.size()) {
                antColor = m_colonies[ant.colonyId].color;
            }

            registerEffect(new AntEatenEffect(ant.pos, antColor));

            ant.dead = true;

            spider.eatenAnts += 1;
            spider.eatenSinceLastOffspring += 1;

            slot = 35.0 + QRandomGenerator::global()->generateDouble() * 25.0;

            return true;
        }
    }

    return false;
}

int GameWorld::spiderUsedSlots(const Spider& spider) const
{
    int used = 0;

    for (double slot : spider.digestionSlots) {
        if (slot > 0.0) {
            ++used;
        }
    }

    return used;
}

double GameWorld::spiderFullnessRatio(const Spider& spider) const
{
    if (spider.digestionSlots.isEmpty()) {
        return 0.0;
    }

    return static_cast<double>(spiderUsedSlots(spider))
           / static_cast<double>(spider.digestionSlots.size());
}

double GameWorld::spiderSpeedFactor(const Spider& spider) const
{
    const double fullness = spiderFullnessRatio(spider);

    // 0.0 voll? nein: 100% Tempo
    // 1.0 komplett voll: 35% Tempo
    return std::clamp(1.0 - fullness * 0.65, 0.35, 1.0);
}

int GameWorld::nearestLiveAntIndex(const QPointF& pos, double radiusPx) const
{
    int bestIndex = -1;
    double bestDist = radiusPx * radiusPx;

    for (int i = 0; i < m_ants.size(); ++i) {
        const Ant& ant = m_ants[i];

        if (ant.dead) {
            continue;
        }

        const double d = distanceSquared(pos, ant.pos);

        if (d < bestDist) {
            bestDist = d;
            bestIndex = i;
        }
    }

    return bestIndex;
}



QPointF GameWorld::steerSpider(
    const Spider& spider,
    const QPointF& desiredDir
    ) const
{
    const QPointF baseDir = normalized(
        blendDirections(spider.dir, 0.35, desiredDir, 0.65)
        );

    const double angles[] = {
        0.0,
        -0.25, 0.25,
        -0.55, 0.55,
        -0.90, 0.90,
        -1.35, 1.35,
        -1.90, 1.90,
        3.14
    };

    double bestScore = -1000000000.0;
    QPointF bestDir = baseDir;
    bool found = false;

    for (double angle : angles) {
        const QPointF dir = normalized(rotated(baseDir, angle));

        const QPointF nearProbe(
            spider.pos.x() + dir.x() * 7.0,
            spider.pos.y() + dir.y() * 7.0
            );

        const QPointF midProbe(
            spider.pos.x() + dir.x() * 18.0,
            spider.pos.y() + dir.y() * 18.0
            );

        const QPointF farProbe(
            spider.pos.x() + dir.x() * 34.0,
            spider.pos.y() + dir.y() * 34.0
            );

        if (!m_map.isWalkableWorld(nearProbe)) {
            continue;
        }

        found = true;

        double score = 0.0;

        if (m_map.isWalkableWorld(midProbe)) {
            score += 2.4;
        } else {
            score -= 4.0;
        }

        if (m_map.isWalkableWorld(farProbe)) {
            score += 1.7;
        }

        score += dot(dir, normalized(desiredDir)) * 3.2;
        score += dot(dir, normalized(spider.dir)) * 0.8;

        score += (QRandomGenerator::global()->generateDouble() - 0.5) * 0.12;

        if (score > bestScore) {
            bestScore = score;
            bestDir = dir;
        }
    }

    if (!found) {
        return normalized(rotated(baseDir, Pi));
    }

    return normalized(bestDir);
}

void GameWorld::spawnFoodAt(const QPointF& pos, int amount)
{
    if (amount <= 0) {
        return;
    }

    FoodSource source;
    source.pos = pos;
    source.amount = amount;

    m_food.push_back(source);
}

void GameWorld::removeDepletedFoodSources()
{
    bool hasDepletedSources = false;
    for (const FoodSource& food : m_food) {
        if (food.amount <= 0) {
            hasDepletedSources = true;
            break;
        }
    }

    if (!hasDepletedSources) {
        return;
    }

    QVector<int> remappedIndices(m_food.size(), -1);
    QVector<FoodSource> activeFood;
    activeFood.reserve(m_food.size());

    for (int oldIndex = 0; oldIndex < m_food.size(); ++oldIndex) {
        const FoodSource& food = m_food[oldIndex];
        if (food.amount <= 0) {
            continue;
        }

        remappedIndices[oldIndex] = activeFood.size();
        activeFood.push_back(food);
    }

    m_food.swap(activeFood);

    for (Ant& ant : m_ants) {
        const int oldTargetIndex = ant.searchTargetFoodIndex;
        if (oldTargetIndex < 0) {
            continue;
        }

        if (oldTargetIndex >= remappedIndices.size()
            || remappedIndices[oldTargetIndex] < 0) {
            resetSearchMemory(ant);
            continue;
        }

        ant.searchTargetFoodIndex = remappedIndices[oldTargetIndex];
    }
}

bool GameWorld::spiderWantsToReproduce(const Spider& spider) const
{
    constexpr int eatenNeededForOffspring = 8; // 12

    if (spider.dead) {
        return false;
    }

    if (spider.reproductionCooldown > 0.0) {
        return false;
    }

    if (spider.eatenSinceLastOffspring < eatenNeededForOffspring) {
        return false;
    }

    // Sehr junge Spinnen sollen nicht sofort Nachwuchs bekommen.
    if (spider.ageSeconds < 45.0) {
        return false;
    }

    return true;
}

void GameWorld::updateNestHealth(double dt)
{
    if (dt <= 0.0) {
        return;
    }

    const double repair = NestRepairPerSecond * dt;

    for (Colony& colony : m_colonies) {
        if (colony.dead || colony.destroyed || colony.nestHealth <= 0.0 ||
            colony.nestHealth >= MaximumNestHealth) {
            continue;
        }

        colony.nestHealth = std::min(
            MaximumNestHealth,
            colony.nestHealth + repair
            );
    }

    for (SpiderHole& hole : m_spiderHoles) {
        if (hole.health <= 0.0 || hole.health >= MaximumNestHealth) {
            continue;
        }

        hole.health = std::min(MaximumNestHealth, hole.health + repair);
    }
}

void GameWorld::updateColonyDeathStates()
{
    for (Colony& colony : m_colonies) {
        if (colony.dead) {
            continue;
        }

        if (colony.antCount <= 0) {
            colony.antCount = 0;
            colony.dead = true;
            colony.deathTimeSeconds = m_worldTimeSeconds;
            m_soundEvents.push_back({ SoundEffect::ColonyDied, colony.home });
        }
    }
}

void GameWorld::removeExpiredColonies()
{
    bool removedColony = false;

    for (int colonyId = m_colonies.size() - 1; colonyId >= 0; --colonyId) {
        const Colony& colony = m_colonies[colonyId];

        if (!colony.dead || colony.deathTimeSeconds < 0.0 ||
            m_worldTimeSeconds - colony.deathTimeSeconds <
                ColonyCleanupDelaySeconds) {
            continue;
        }

        for (int antIndex = m_ants.size() - 1; antIndex >= 0; --antIndex) {
            Ant& ant = m_ants[antIndex];

            if (ant.colonyId == colonyId) {
                if (ant.carryingFood) {
                    spawnFoodAt(ant.pos, 1);
                }
                m_ants.removeAt(antIndex);
            } else if (ant.colonyId > colonyId) {
                --ant.colonyId;
            }
        }

        m_colonies.removeAt(colonyId);
        removedColony = true;
    }

    if (removedColony) {
        rebuildHomeFlowFields();
    }
}

QPointF GameWorld::findNearbyWalkablePosition(
    const QPointF& origin,
    double minRadius,
    double maxRadius
    ) const
{
    constexpr int attempts = 36;

    for (int i = 0; i < attempts; ++i) {
        const double angle =
            QRandomGenerator::global()->generateDouble() * Pi * 2.0;

        const double radius =
            minRadius
            + QRandomGenerator::global()->generateDouble() * (maxRadius - minRadius);

        const QPointF candidate(
            origin.x() + std::cos(angle) * radius,
            origin.y() + std::sin(angle) * radius
            );

        if (m_map.isWalkableWorld(candidate)) {
            return candidate;
        }
    }

    // Fallback: Elternposition. Die ist normalerweise walkable.
    return origin;
}

void GameWorld::removeDeadSpiders()
{
    for (int i = m_spiders.size() - 1; i >= 0; --i) {
        if (m_spiders[i].dead) {
            m_spiders.removeAt(i);
        }
    }
}

bool GameWorld::tryMoveSpider(
    Spider& spider,
    const QPointF& dir,
    double step
    ) const
{
    if (step <= 0.0) {
        return false;
    }

    const QPointF next(
        spider.pos.x() + dir.x() * step,
        spider.pos.y() + dir.y() * step
        );

    if (!m_map.isWalkableWorld(next)) {
        return false;
    }

    spider.pos = next;
    return true;
}
