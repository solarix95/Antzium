#pragma once

#include <QElapsedTimer>
#include <QPoint>
#include <QPointF>
#include <QSet>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include <QRectF>
#include <QImage>

#include "gameworld.h"

class GameWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GameWidget(QWidget* parent = nullptr);

    bool loadMapFromFile(const QString& fileName);
    bool loadMapFromImage(const QImage& image);
    bool saveBaseMapPng(const QString& fileName) const;
    bool saveStateJson(const QString& fileName) const;
    bool loadStateJson(const QString& fileName);

    void startDemo();
    bool loadMap();

    const GameWorld& world() const;
    QRectF cameraWorldRect() const;
    double simulationSpeed() const;
    double virtualTimeSeconds() const;
    int hoveredColonyId() const;

    void setCameraCenterWorld(const QPointF& center);

    // Userinteraction
    enum class InteractionTool
    {
        Pointer,
        Magnifier,
        Food,
        Pheromone,
        Dynamite,
        Flower,
        Grass,
        Rock
    };

    InteractionTool interactionTool() const;

signals:
    void worldChanged();
    void worldStatsChanged();
    void viewChanged();
    void simulationSpeedChanged(double speed);
    void simulationTimeChanged(double seconds);
    void interactionToolChanged(GameWidget::InteractionTool tool);
    void hoveredColonyChanged(int colonyId);
    void soundEffectRequested(SoundEffect effect, const QPointF& worldPosition);

public slots:
    void setSimulationSpeed(double speed);
    void setInteractionTool(InteractionTool tool);
    void setHoveredColonyId(int colonyId);
    void setColonyColor(int colonyId, const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void onFrame();

private:
    void updateCamera(double dt);
    void clampCamera();
    QPointF widgetToWorld(const QPoint& widgetPos) const;
    int colonyAtWidgetPosition(const QPoint& widgetPos) const;
    void applyActiveToolAt(const QPoint& widgetPos);
    void cycleInteractionTool();
    void dispatchSoundEvents();
    void resetWeather();
    void updateWeather(double dt);
    void updateAmbientSound(double dt);
    QRectF mapViewportRect() const;
    void ensureRainDrops();
    void resetRainDrop(int index, bool placeInsideViewport);
    void drawRain(QPainter& painter) const;
    void drawHoveredColony(QPainter& painter) const;

private:
    GameWorld m_world;

    QTimer m_timer;
    QElapsedTimer m_frameClock;

    struct MeanTime {
        QVector<int> values;
        inline void addValue(int v) { values.push_back(v); if (values.count() > 10) values.removeFirst(); }
        inline int  meanValue() const { if (values.isEmpty()) return 0; int ret = 0; for (int v : values) ret += v; return ret/values.count();}

    };

    MeanTime m_lastFrameTickMs;
    MeanTime m_lastFramePaintMs;

    QPointF m_camera;
    QPoint m_mousePos;
    bool m_mouseInside = false;
    int m_hoveredColonyId = -1;
    QSet<int> m_pressedKeys;

    double m_simulationSpeed = 1.0;
    double m_virtualTimeSeconds = 0.0;
    double m_simulationAccumulator = 0.0;

    enum class WeatherState
    {
        Dry,
        Raining
    };

    struct RainDrop
    {
        QPointF position;
        double verticalSpeed = 0.0;
        double horizontalSpeed = 0.0;
        double length = 0.0;
        double opacity = 0.0;
    };

    WeatherState m_weatherState = WeatherState::Dry;
    double m_weatherTimeRemaining = 0.0;
    double m_ambientSoundTimeRemaining = 0.0;
    double m_rainIntensity = 0.0;
    QVector<RainDrop> m_rainDrops;

    // Tooling/Interaction
    InteractionTool m_interactionTool = InteractionTool::Pointer;
    double m_magnifierBurnRadius = 48.0;
    int m_foodPlacementAmount = 160;
    double m_pheromoneBrushRadius = 32.0;
    double m_pheromoneBrushAmount = 75.0;
    double m_dynamitePreviewRadius = 70.0;
    double m_terrainBrushRadius = 26.0;

};
