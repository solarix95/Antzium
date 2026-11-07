#include "gamewidget.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
constexpr double DryWeatherSpeedFactor = 1.0;
constexpr double RainWeatherSpeedFactor = 0.7;
constexpr double WeatherTransitionSeconds = 5.0;

double randomBetween(double minimum, double maximum)
{
    return minimum
        + QRandomGenerator::global()->generateDouble() * (maximum - minimum);
}
}

GameWidget::GameWidget(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    connect(&m_timer, &QTimer::timeout, this, &GameWidget::onFrame);

    m_frameClock.start();
    m_timer.start(16);
}

const GameWorld& GameWidget::world() const
{
    return m_world;
}

QRectF GameWidget::cameraWorldRect() const
{
    return QRectF(m_camera, QSizeF(width(), height()));
}

void GameWidget::setCameraCenterWorld(const QPointF& center)
{
    m_camera = QPointF(
        center.x() - width() * 0.5,
        center.y() - height() * 0.5
        );

    clampCamera();
    if (m_mouseInside) {
        setHoveredColonyId(colonyAtWidgetPosition(m_mousePos));
    }
    update();

    emit viewChanged();
}

bool GameWidget::loadMapFromFile(const QString& fileName)
{
    if (!m_world.loadMap(fileName, 10)) {
        return false;
    }
    return loadMap();
}

bool GameWidget::loadMapFromImage(const QImage& image)
{
    if (!m_world.loadMapFromImage(image, 10)) {
        return false;
    }

    return loadMap();
}

bool GameWidget::saveBaseMapPng(const QString& fileName) const
{
    return m_world.saveBaseMapPng(fileName);
}

bool GameWidget::saveStateJson(const QString& fileName) const
{
    return m_world.saveStateJson(fileName);
}

bool GameWidget::loadStateJson(const QString& fileName)
{
    if (!m_world.loadStateJson(fileName)) {
        return false;
    }

    m_camera = QPointF(0, 0);
    m_virtualTimeSeconds = m_world.worldTimeSeconds();
    m_simulationAccumulator = 0.0;
    setHoveredColonyId(-1);
    resetWeather();

    update();

    emit worldChanged();
    emit worldStatsChanged();
    emit viewChanged();
    emit simulationTimeChanged(m_virtualTimeSeconds);

    return true;
}

void GameWidget::startDemo()
{
    m_world.createFallback();
    loadMap();
}

bool GameWidget::loadMap()
{
    if (!m_world.hasMap())
        return false;

    m_camera = QPointF(0, 0);

    m_virtualTimeSeconds = 0.0;
    m_simulationAccumulator = 0.0;
    setHoveredColonyId(-1);
    resetWeather();

    update();

    emit worldChanged();
    emit worldStatsChanged();
    emit viewChanged();
    emit simulationTimeChanged(m_virtualTimeSeconds);
    return true;
}

void GameWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);

    painter.fillRect(rect(), QColor(20, 20, 20));

    if (!m_world.hasMap()) {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, "Keine Map geladen");
        return;
    }

    QElapsedTimer renderTimer;
    renderTimer.start();

    QRectF cameraRect(m_camera, QSizeF(width(), height()));

    painter.save();
    painter.translate(-m_camera.x(), -m_camera.y());

    painter.setRenderHint(QPainter::Antialiasing, false);
    m_world.map().draw(painter, cameraRect);

    m_world.drawEntities(painter, cameraRect);

    painter.restore();

    drawRain(painter);
    drawHoveredColony(painter);

    if (m_world.hasMap()) {
        painter.setRenderHint(QPainter::Antialiasing, true);

        switch (m_interactionTool) {
        case InteractionTool::Pointer:
            break;

        case InteractionTool::Magnifier:
            painter.setPen(QPen(QColor(255, 230, 120, 180), 2));
            painter.setBrush(QColor(255, 210, 60, 35));

            painter.drawEllipse(
                QPointF(m_mousePos.x(), m_mousePos.y()),
                m_magnifierBurnRadius,
                m_magnifierBurnRadius
                );
            break;

        case InteractionTool::Food:
            painter.setPen(QPen(QColor(255, 230, 90, 180), 2));
            painter.setBrush(QColor(230, 190, 40, 70));

            painter.drawEllipse(
                QPointF(m_mousePos.x(), m_mousePos.y()),
                13.0,
                13.0
                );
            break;

        case InteractionTool::Pheromone:
            painter.setPen(QPen(QColor(255, 190, 40, 190), 2));
            painter.setBrush(QColor(255, 190, 40, 35));

            painter.drawEllipse(
                QPointF(m_mousePos.x(), m_mousePos.y()),
                m_pheromoneBrushRadius,
                m_pheromoneBrushRadius
                );
            break;

        case InteractionTool::Dynamite:
            painter.setPen(QPen(QColor(220, 45, 35, 200), 2));
            painter.setBrush(QColor(220, 45, 35, 38));

            painter.drawEllipse(
                QPointF(m_mousePos.x(), m_mousePos.y()),
                m_dynamitePreviewRadius,
                m_dynamitePreviewRadius
                );
            break;

        case InteractionTool::Flower:
            painter.setPen(QPen(QColor(180, 70, 220, 200), 2));
            painter.setBrush(QColor(190, 80, 230, 70));

            painter.drawEllipse(
                QPointF(m_mousePos.x(), m_mousePos.y()),
                12.0,
                12.0
                );
            break;

        case InteractionTool::Grass:
            painter.setPen(QPen(QColor(70, 165, 70, 200), 2));
            painter.setBrush(QColor(80, 170, 70, 55));

            painter.drawEllipse(
                QPointF(m_mousePos.x(), m_mousePos.y()),
                m_terrainBrushRadius,
                m_terrainBrushRadius
                );
            break;

        case InteractionTool::Rock:
            painter.setPen(QPen(QColor(120, 120, 120, 210), 2));
            painter.setBrush(QColor(110, 110, 110, 60));

            painter.drawEllipse(
                QPointF(m_mousePos.x(), m_mousePos.y()),
                m_terrainBrushRadius,
                m_terrainBrushRadius
                );
            break;
        }
    }

    m_lastFramePaintMs.addValue(renderTimer.elapsed());
    painter.setPen(Qt::white);
    int tickTime  = m_lastFrameTickMs.meanValue();
    int paintTime = m_lastFramePaintMs.meanValue();
    int fps = qMin(tickTime + paintTime > 0 ? 1000/(tickTime + paintTime) : 100,100);
    painter.drawText(12, 22, QString("FPS: %1 (%2/%3)").arg(fps).arg(tickTime).arg(paintTime));

    const QString antCountText = QStringLiteral("★ %1")
                                     .arg(m_world.totalAntCount());
    QFont antCountFont = painter.font();
    antCountFont.setBold(true);
    antCountFont.setPointSize(antCountFont.pointSize() + 1);
    painter.setFont(antCountFont);

    const QFontMetrics antCountMetrics(antCountFont);
    const QSize antCountTextSize = antCountMetrics.size(
        Qt::TextSingleLine,
        antCountText
        );
    const QRectF antCountBadge(
        (width() - antCountTextSize.width() - 20.0) * 0.5,
        6.0,
        antCountTextSize.width() + 20.0,
        antCountTextSize.height() + 8.0
        );

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(230, 190, 40, 210), 1.25));
    painter.setBrush(QColor(20, 24, 29, 205));
    painter.drawRoundedRect(antCountBadge, 5.0, 5.0);
    painter.setPen(QColor(245, 225, 125));
    painter.drawText(antCountBadge, Qt::AlignCenter, antCountText);
    // painter.drawText(12, 42, "Pfeiltasten oder Maus an den Rand bewegen");
}

void GameWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F12) {
        event->ignore();
        return;
    }

    if (!event->isAutoRepeat()) {
        m_pressedKeys.insert(event->key());
    }

    QWidget::keyPressEvent(event);
}

void GameWidget::keyReleaseEvent(QKeyEvent* event)
{
    if (!event->isAutoRepeat()) {
        m_pressedKeys.remove(event->key());
    }

    QWidget::keyReleaseEvent(event);
}

void GameWidget::mouseMoveEvent(QMouseEvent* event)
{
    m_mouseInside = true;
    m_mousePos = event->pos();
    setHoveredColonyId(colonyAtWidgetPosition(m_mousePos));

    if ((event->buttons() & Qt::LeftButton) &&
        (m_interactionTool == InteractionTool::Magnifier ||
         m_interactionTool == InteractionTool::Pheromone ||
         m_interactionTool == InteractionTool::Grass ||
         m_interactionTool == InteractionTool::Rock)) {
        applyActiveToolAt(event->pos());
    }

    QWidget::mouseMoveEvent(event);
}

void GameWidget::leaveEvent(QEvent* event)
{
    m_mouseInside = false;
    setHoveredColonyId(-1);
    QWidget::leaveEvent(event);
}

void GameWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        cycleInteractionTool();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        applyActiveToolAt(event->pos());
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void GameWidget::onFrame()
{
    const double realDt = std::min(0.05, m_frameClock.restart() / 1000.0);


    // Kamera bleibt echte Zeit, damit Scrolling bei 20x nicht unkontrollierbar wird.
    updateCamera(realDt);
    if (m_mouseInside) {
        setHoveredColonyId(colonyAtWidgetPosition(m_mousePos));
    }
    updateWeather(m_simulationSpeed > 0.0 ? realDt : 0.0);

    bool simulationAdvanced = false;

    if (m_world.hasMap() && m_simulationSpeed > 0.0) {
        const double virtualDt = realDt * m_simulationSpeed;

        m_simulationAccumulator += virtualDt;

        // Kleiner Fixschritt: stabiler für Kollisionen, Pheromone und Wall-Following.
        constexpr double fixedStep = 1.0 / 30.0;

        // Schutz gegen zu viel CPU-Last bei sehr hohen Faktoren.
        constexpr int maxStepsPerFrame = 80;

        int steps = 0;

        while (m_simulationAccumulator >= fixedStep && steps < maxStepsPerFrame) {
            m_world.tick(fixedStep);

            m_simulationAccumulator -= fixedStep;
            m_virtualTimeSeconds += fixedStep;

            ++steps;
            simulationAdvanced = true;
        }

        dispatchSoundEvents();

        // Falls der Rechner nicht hinterherkommt, nicht endlos Rückstand aufbauen.
        if (steps >= maxStepsPerFrame) {
            m_simulationAccumulator = 0.0;
        }
    }

    m_lastFrameTickMs.addValue(m_frameClock.elapsed());
    update();

    emit viewChanged();

    if (simulationAdvanced) {
        emit simulationTimeChanged(m_virtualTimeSeconds);
        emit worldStatsChanged();
    }
}

void GameWidget::resetWeather()
{
    m_weatherState = WeatherState::Dry;
    m_weatherTimeRemaining = randomBetween(18.0, 38.0);
    m_ambientSoundTimeRemaining = randomBetween(20.0, 50.0);
    m_rainIntensity = 0.0;
    m_rainDrops.clear();
    m_world.setWeatherSpeedFactor(DryWeatherSpeedFactor);
}

void GameWidget::updateWeather(double dt)
{
    if (!m_world.hasMap() || dt <= 0.0) {
        return;
    }

    m_weatherTimeRemaining -= dt;
    if (m_weatherTimeRemaining <= 0.0) {
        if (m_weatherState == WeatherState::Dry) {
            m_weatherState = WeatherState::Raining;
            m_weatherTimeRemaining = randomBetween(18.0, 34.0);
            ensureRainDrops();
        } else {
            m_weatherState = WeatherState::Dry;
            m_weatherTimeRemaining = randomBetween(55.0, 130.0);
        }
    }

    const double targetIntensity =
        m_weatherState == WeatherState::Raining ? 1.0 : 0.0;
    constexpr double fadeSpeed = 1.0 / WeatherTransitionSeconds;
    const double intensityStep = fadeSpeed * dt;

    if (m_rainIntensity < targetIntensity) {
        m_rainIntensity = std::min(targetIntensity, m_rainIntensity + intensityStep);
    } else if (m_rainIntensity > targetIntensity) {
        m_rainIntensity = std::max(targetIntensity, m_rainIntensity - intensityStep);
    }

    const double weatherSpeedFactor =
        DryWeatherSpeedFactor
        - (DryWeatherSpeedFactor - RainWeatherSpeedFactor) * m_rainIntensity;
    m_world.setWeatherSpeedFactor(weatherSpeedFactor);
    updateAmbientSound(dt);

    if (m_rainIntensity <= 0.0) {
        return;
    }

    ensureRainDrops();

    const QRectF rainRect = mapViewportRect();
    if (rainRect.isEmpty()) {
        m_rainDrops.clear();
        return;
    }

    for (int i = 0; i < m_rainDrops.size(); ++i) {
        RainDrop& drop = m_rainDrops[i];
        drop.position.rx() += drop.horizontalSpeed * dt;
        drop.position.ry() += drop.verticalSpeed * dt;

        if (drop.position.y() > rainRect.bottom() + drop.length
            || drop.position.x() < rainRect.left() - drop.length) {
            resetRainDrop(i, false);
        }
    }
}

void GameWidget::updateAmbientSound(double dt)
{
    m_ambientSoundTimeRemaining -= dt;
    if (m_ambientSoundTimeRemaining > 0.0) {
        return;
    }

    m_ambientSoundTimeRemaining = randomBetween(20.0, 50.0);

    static constexpr std::array<SoundEffect, 3> rainSounds = {{
        SoundEffect::AmbientThunder1,
        SoundEffect::AmbientThunder2,
        SoundEffect::AmbientThunder3
    }};
    static constexpr std::array<SoundEffect, 4> clearSounds = {{
        SoundEffect::AmbientCricket1,
        SoundEffect::AmbientBird1,
        SoundEffect::AmbientCricket2,
        SoundEffect::AmbientBird2
    }};

    SoundEffect effect = SoundEffect::AmbientThunder1;
    if (m_weatherState == WeatherState::Raining) {
        effect = rainSounds[QRandomGenerator::global()->bounded(
            static_cast<int>(rainSounds.size())
            )];
    } else {
        effect = clearSounds[QRandomGenerator::global()->bounded(
            static_cast<int>(clearSounds.size())
            )];
    }

    emit soundEffectRequested(effect, cameraWorldRect().center());
}

QRectF GameWidget::mapViewportRect() const
{
    if (!m_world.hasMap()) {
        return QRectF();
    }

    const QRectF mapRect(
        QPointF(-m_camera.x(), -m_camera.y()),
        QSizeF(m_world.map().worldSizePx())
        );

    return mapRect.intersected(QRectF(rect()));
}

void GameWidget::ensureRainDrops()
{
    const QRectF rainRect = mapViewportRect();
    if (rainRect.isEmpty()) {
        m_rainDrops.clear();
        return;
    }

    const int rainArea = std::max(
        1,
        static_cast<int>(std::round(rainRect.width() * rainRect.height()))
        );
    const int targetCount = std::clamp(rainArea / 4500, 24, 260);

    while (m_rainDrops.size() < targetCount) {
        m_rainDrops.push_back(RainDrop{});
        resetRainDrop(m_rainDrops.size() - 1, true);
    }

    if (m_rainDrops.size() > targetCount) {
        m_rainDrops.resize(targetCount);
    }
}

void GameWidget::resetRainDrop(int index, bool placeInsideViewport)
{
    if (index < 0 || index >= m_rainDrops.size()) {
        return;
    }

    const QRectF rainRect = mapViewportRect();
    if (rainRect.isEmpty()) {
        return;
    }

    RainDrop& drop = m_rainDrops[index];
    const double rainHeight = std::max(1.0, rainRect.height());

    drop.position.setX(randomBetween(rainRect.left() - 20.0, rainRect.right() + 100.0));
    drop.position.setY(
        placeInsideViewport
            ? randomBetween(rainRect.top() - 0.15 * rainHeight, rainRect.bottom())
            : randomBetween(rainRect.top() - 120.0, rainRect.top() - 10.0)
        );
    drop.verticalSpeed = randomBetween(620.0, 980.0);
    drop.horizontalSpeed = randomBetween(-175.0, -95.0);
    drop.length = randomBetween(9.0, 18.0);
    drop.opacity = randomBetween(0.35, 0.85);
}

void GameWidget::drawRain(QPainter& painter) const
{
    if (m_rainIntensity <= 0.0) {
        return;
    }

    const QRectF rainRect = mapViewportRect();
    if (rainRect.isEmpty()) {
        return;
    }

    painter.save();
    painter.setClipRect(rainRect);

    // Die neutrale Ebene reduziert die Farbsättigung günstig, ohne die Szene
    // für jeden Frame in ein zusätzliches Bild rendern zu müssen.
    painter.fillRect(
        rainRect,
        QColor(128, 130, 132, static_cast<int>(std::round(48.0 * m_rainIntensity)))
        );
    painter.fillRect(
        rainRect,
        QColor(42, 58, 70, static_cast<int>(std::round(24.0 * m_rainIntensity)))
        );
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const RainDrop& drop : m_rainDrops) {
        const int alpha = static_cast<int>(
            std::round(185.0 * drop.opacity * m_rainIntensity)
            );
        painter.setPen(QPen(QColor(185, 215, 235, alpha), 1.15));

        const double horizontalTail =
            -drop.horizontalSpeed / drop.verticalSpeed * drop.length;
        const QPointF tail(
            drop.position.x() + horizontalTail,
            drop.position.y() - drop.length
            );
        painter.drawLine(drop.position, tail);
    }

    painter.restore();
}

void GameWidget::drawHoveredColony(QPainter& painter) const
{
    if (m_hoveredColonyId < 0
        || m_hoveredColonyId >= m_world.colonyCount()) {
        return;
    }

    const QPointF center = m_world.colonyHome(m_hoveredColonyId) - m_camera;
    if (!rect().adjusted(-50, -50, 50, 50).contains(center.toPoint())) {
        return;
    }

    QColor colonyColor = m_world.colonyColor(m_hoveredColonyId);
    QColor glowColor = colonyColor;
    glowColor.setAlpha(65);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(glowColor);
    painter.setPen(QPen(QColor(255, 255, 255, 210), 1.5));
    painter.drawEllipse(center, 31.0, 31.0);

    colonyColor.setAlpha(245);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(colonyColor, 3.0));
    painter.drawEllipse(center, 26.0, 26.0);

    const QString scoreText = QStringLiteral("★ %1")
                                  .arg(m_world.colonyScore(m_hoveredColonyId));
    const QFontMetrics metrics(painter.font());
    const QSize textSize = metrics.size(Qt::TextSingleLine, scoreText);
    QRectF badge(
        center.x() - textSize.width() * 0.5 - 7.0,
        center.y() - 49.0,
        textSize.width() + 14.0,
        textSize.height() + 6.0
        );

    badge.moveLeft(std::clamp(badge.left(), 3.0, width() - badge.width() - 3.0));
    badge.moveTop(std::clamp(badge.top(), 3.0, height() - badge.height() - 3.0));

    painter.setPen(QPen(colonyColor, 1.5));
    painter.setBrush(QColor(20, 24, 29, 225));
    painter.drawRoundedRect(badge, 4.0, 4.0);
    painter.setPen(QColor(245, 247, 250));
    painter.drawText(badge, Qt::AlignCenter, scoreText);
    painter.restore();
}

void GameWidget::updateCamera(double dt)
{
    if (!m_world.hasMap()) {
        return;
    }

    const double speed = 450.0;
    const int edge = 24;
    const bool edgeScrollEnabled = m_mouseInside;

    QPointF delta(0, 0);

    if (m_pressedKeys.contains(Qt::Key_Left) || m_pressedKeys.contains(Qt::Key_A) ||
        (edgeScrollEnabled && m_mousePos.x() <= edge)) {
        delta.rx() -= speed * dt;
    }

    if (m_pressedKeys.contains(Qt::Key_Right) || m_pressedKeys.contains(Qt::Key_D) ||
        (edgeScrollEnabled && m_mousePos.x() >= width() - edge)) {
        delta.rx() += speed * dt;
    }

    if (m_pressedKeys.contains(Qt::Key_Up) || m_pressedKeys.contains(Qt::Key_W) ||
        (edgeScrollEnabled && m_mousePos.y() <= edge)) {
        delta.ry() -= speed * dt;
    }

    if (m_pressedKeys.contains(Qt::Key_Down) || m_pressedKeys.contains(Qt::Key_S) ||
        (edgeScrollEnabled && m_mousePos.y() >= height() - edge)) {
        delta.ry() += speed * dt;
    }

    m_camera += delta;
    clampCamera();
}

void GameWidget::clampCamera()
{
    if (!m_world.hasMap()) {
        return;
    }

    const QSize worldSize = m_world.map().worldSizePx();

    const double maxX = std::max(0, worldSize.width() - width());
    const double maxY = std::max(0, worldSize.height() - height());

    m_camera.setX(std::clamp(m_camera.x(), 0.0, maxX));
    m_camera.setY(std::clamp(m_camera.y(), 0.0, maxY));
}

GameWidget::InteractionTool GameWidget::interactionTool() const
{
    return m_interactionTool;
}

void GameWidget::setInteractionTool(InteractionTool tool)
{
    if (m_interactionTool == tool) {
        return;
    }

    m_interactionTool = tool;
    update();

    emit interactionToolChanged(m_interactionTool);
}

QPointF GameWidget::widgetToWorld(const QPoint& widgetPos) const
{
    return QPointF(
        m_camera.x() + widgetPos.x(),
        m_camera.y() + widgetPos.y()
        );
}

int GameWidget::colonyAtWidgetPosition(const QPoint& widgetPos) const
{
    if (!m_world.hasMap()) {
        return -1;
    }

    constexpr double hoverRadius = 30.0;
    constexpr double hoverRadiusSquared = hoverRadius * hoverRadius;
    const QPointF worldPosition = widgetToWorld(widgetPos);

    int nearestColonyId = -1;
    double nearestDistanceSquared = hoverRadiusSquared;

    for (int colonyId = 0; colonyId < m_world.colonyCount(); ++colonyId) {
        if (m_world.colonyIsDestroyed(colonyId)) {
            continue;
        }

        const QPointF delta = m_world.colonyHome(colonyId) - worldPosition;
        const double distanceSquared = QPointF::dotProduct(delta, delta);
        if (distanceSquared <= nearestDistanceSquared) {
            nearestDistanceSquared = distanceSquared;
            nearestColonyId = colonyId;
        }
    }

    return nearestColonyId;
}

void GameWidget::applyActiveToolAt(const QPoint& widgetPos)
{
    if (!m_world.hasMap()) {
        return;
    }

    const QPointF worldPos = widgetToWorld(widgetPos);

    switch (m_interactionTool) {
    case InteractionTool::Pointer:
        break;

    case InteractionTool::Magnifier:
        m_world.burnAt(worldPos, m_magnifierBurnRadius);
        update();
        emit viewChanged();
        emit worldStatsChanged();
        break;

    case InteractionTool::Food:
        m_world.placeFoodAt(worldPos, m_foodPlacementAmount);

        update();

        emit viewChanged();
        emit worldStatsChanged();
        break;

    case InteractionTool::Pheromone:

        m_world.placePheromoneAt(
            worldPos,
            m_pheromoneBrushRadius,
            m_pheromoneBrushAmount
            );

        update();

        emit viewChanged();
        emit worldStatsChanged();

        break;

    case InteractionTool::Dynamite:
        m_world.placeDynamiteAt(worldPos);

        update();

        emit viewChanged();
        emit worldStatsChanged();

        break;

    case InteractionTool::Flower:
        m_world.placeFlowerAt(worldPos);

        update();

        emit viewChanged();
        emit worldStatsChanged();

        break;

    case InteractionTool::Grass:
        m_world.placeGrassAt(worldPos, m_terrainBrushRadius);

        update();

        emit viewChanged();
        emit worldStatsChanged();

        break;

    case InteractionTool::Rock:
        m_world.placeRockAt(worldPos, m_terrainBrushRadius);

        update();

        emit viewChanged();
        emit worldStatsChanged();

        break;
    }

    dispatchSoundEvents();


}

void GameWidget::dispatchSoundEvents()
{
    const QVector<SoundEvent> events = m_world.takeSoundEvents();
    for (const SoundEvent& event : events) {
        emit soundEffectRequested(event.effect, event.worldPosition);
    }
}

void GameWidget::cycleInteractionTool()
{
    switch (m_interactionTool) {
    case InteractionTool::Pointer:
        setInteractionTool(InteractionTool::Magnifier);
        break;

    case InteractionTool::Magnifier:
        setInteractionTool(InteractionTool::Food);
        break;

    case InteractionTool::Food:
        setInteractionTool(InteractionTool::Pheromone);
        break;

    case InteractionTool::Pheromone:
        setInteractionTool(InteractionTool::Dynamite);
        break;

    case InteractionTool::Dynamite:
        setInteractionTool(InteractionTool::Flower);
        break;

    case InteractionTool::Flower:
        setInteractionTool(InteractionTool::Grass);
        break;

    case InteractionTool::Grass:
        setInteractionTool(InteractionTool::Rock);
        break;

    case InteractionTool::Rock:
        setInteractionTool(InteractionTool::Pointer);
        break;
    }
}

double GameWidget::simulationSpeed() const
{
    return m_simulationSpeed;
}

double GameWidget::virtualTimeSeconds() const
{
    return m_virtualTimeSeconds;
}

int GameWidget::hoveredColonyId() const
{
    return m_hoveredColonyId;
}

void GameWidget::setHoveredColonyId(int colonyId)
{
    if (colonyId < 0 || colonyId >= m_world.colonyCount()) {
        colonyId = -1;
    }

    if (m_hoveredColonyId == colonyId) {
        return;
    }

    m_hoveredColonyId = colonyId;
    update();
    emit hoveredColonyChanged(m_hoveredColonyId);
}

void GameWidget::setColonyColor(int colonyId, const QColor& color)
{
    if (!m_world.setColonyColor(colonyId, color)) {
        return;
    }

    update();
    emit worldStatsChanged();
    emit viewChanged();
}

void GameWidget::setSimulationSpeed(double speed)
{
    const double clamped = std::clamp(speed, 0.0, 50.0);

    if (std::abs(m_simulationSpeed - clamped) < 0.0001) {
        return;
    }

    m_simulationSpeed = clamped;
    emit simulationSpeedChanged(m_simulationSpeed);
}
