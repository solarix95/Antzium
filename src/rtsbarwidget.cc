

#include <algorithm>
#include <cmath>
#include <memory>

#include <QButtonGroup>
#include <QColorDialog>
#include <QContextMenuEvent>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>


#include "rtsbarwidget.h"
#include "audio/antsimaudio.h"
#include "gamewidget.h"
#include "gameworld.h"
#include "minimapwidget.h"
#include "iconfactory.h"

namespace
{

struct ColonyDisplayRow
{
    int colonyId = -1;
    int antCount = 0;
    int storedFood = 0;
    int score = 0;
    bool dead = false;
    bool destroyed = false;
    double nestHealth = 100.0;
    double deathTimeSeconds = -1.0;
    QColor color;
};

QString formatTime(double seconds)
{
    const int totalSeconds = static_cast<int>(seconds);
    const int minutes = totalSeconds / 60;
    const int secs = totalSeconds % 60;

    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
}

void updateColorDot(QLabel* dot, const QColor& color, bool alive)
{
    const int alpha = alive ? 255 : 90;
    QColor displayColor = color;
    displayColor.setAlpha(alpha);

    dot->setStyleSheet(
        QString(
            "background-color: %1;"
            "border-radius: 6px;"
            "border: 1px solid rgba(255,255,255,%2);"
            )
            .arg(displayColor.name(QColor::HexRgb))
            .arg(alive ? 100 : 45)
        );
}

QLabel* makeColorDot(const QColor& color, QWidget* parent, bool alive)
{
    auto* dot = new QLabel(parent);
    dot->setFixedSize(12, 12);
    updateColorDot(dot, color, alive);

    return dot;
}

class ColonyRowWidget final : public QWidget
{
public:
    ColonyRowWidget(int colonyId, GameWidget* game, QWidget* parent)
        : QWidget(parent)
        , m_colonyId(colonyId)
        , m_game(game)
    {
        setMinimumHeight(24);
        setMouseTracking(true);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(4, 1, 4, 1);
        layout->setSpacing(6);

        m_colorDot = makeColorDot(Qt::white, this, true);

        m_scoreLabel = new QLabel(this);
        m_scoreLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_scoreLabel->setMinimumWidth(46);
        m_scoreLabel->setStyleSheet("color: #8fa9bd; font-size: 11px;");

        m_antsLabel = new QLabel(this);
        m_antsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_antsLabel->setMinimumWidth(34);

        m_statusIcon = new QLabel(this);
        m_statusIcon->setFixedSize(8, 8);

        m_statusLabel = new QLabel(this);
        m_statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_statusLabel->setMinimumWidth(36);
        m_statusLabel->setStyleSheet("font-size: 11px;");

        layout->addWidget(m_colorDot);
        layout->addStretch();
        layout->addWidget(m_scoreLabel);
        layout->addWidget(m_antsLabel);
        layout->addSpacing(2);
        layout->addWidget(m_statusIcon);
        layout->addWidget(m_statusLabel);

        if (m_game) {
            connect(m_game, &GameWidget::hoveredColonyChanged, this,
                    [this](int hoveredColonyId) {
                        setHighlighted(hoveredColonyId == m_colonyId);
                    });
            setHighlighted(m_game->hoveredColonyId() == m_colonyId);
        }
    }

    void updateData(const ColonyDisplayRow& data, int rank)
    {
        m_color = data.color;
        const bool alive = !data.dead;
        updateColorDot(m_colorDot, data.color, alive);
        m_scoreLabel->setText(QStringLiteral("★ %1").arg(data.score));

        if (alive) {
            m_scoreLabel->setStyleSheet("color: #8fa9bd; font-size: 11px;");
            m_antsLabel->setText(QString::number(data.antCount));
            m_antsLabel->setStyleSheet(QString());
            m_statusIcon->setStyleSheet(
                "background-color: #E6BE28;"
                "border-radius: 4px;"
                );
            m_statusLabel->setText(QString::number(data.storedFood));
            m_statusLabel->setStyleSheet("color: #E6BE28; font-size: 11px;");
        } else {
            m_scoreLabel->setStyleSheet("color: rgba(143,169,189,120); font-size: 11px;");
            m_antsLabel->setText(QStringLiteral("☠"));
            m_antsLabel->setStyleSheet("color: rgba(215,221,232,130);");
            m_statusIcon->setStyleSheet(QString());
            m_statusIcon->setText(QString());

            const double deathTime = std::max(0.0, data.deathTimeSeconds);
            m_statusLabel->setText(formatTime(deathTime));
            m_statusLabel->setStyleSheet(
                "color: rgba(215,221,232,150); font-size: 11px;"
                );
        }

        setToolTip(
            QStringLiteral(
                "Rang %1 · Punkte %2 · Ameisen %3 · Futter %4 · Nest %5\n"
                "Rechtsklick: Farbe setzen"
                )
                .arg(rank)
                .arg(data.score)
                .arg(data.antCount)
                .arg(data.storedFood)
                .arg(
                    data.destroyed
                        ? QStringLiteral("zerstört")
                        : QStringLiteral("%1 %").arg(qRound(data.nestHealth))
                    )
            );
        update();
    }

    void setHighlighted(bool highlighted)
    {
        if (m_highlighted == highlighted) {
            return;
        }

        m_highlighted = highlighted;
        update();
    }

protected:
    void contextMenuEvent(QContextMenuEvent* event) override
    {
        if (!m_game) {
            event->ignore();
            return;
        }

        const double previousSpeed = m_game->simulationSpeed();
        m_game->setSimulationSpeed(0.0);

        QMenu menu(this);
        QAction* setColorAction = menu.addAction(QStringLiteral("Farbe setzen…"));
        QAction* selectedAction = menu.exec(event->globalPos());

        if (selectedAction == setColorAction && m_game) {
            const QColor selectedColor = QColorDialog::getColor(
                m_color,
                this,
                QStringLiteral("Koloniefarbe setzen")
                );

            if (selectedColor.isValid() && m_game) {
                m_game->setColonyColor(m_colonyId, selectedColor);
            }
        }

        if (m_game) {
            m_game->setSimulationSpeed(previousSpeed);
        }
        event->accept();
    }

    void enterEvent(QEvent* event) override
    {
        if (m_game) {
            m_game->setHoveredColonyId(m_colonyId);
        }
        QWidget::enterEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        if (m_game && m_game->hoveredColonyId() == m_colonyId) {
            m_game->setHoveredColonyId(-1);
        }
        QWidget::leaveEvent(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        QWidget::paintEvent(event);
        if (!m_highlighted) {
            return;
        }

        QColor background = m_color;
        background.setAlpha(45);
        QColor border = m_color;
        border.setAlpha(230);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(background);
        painter.setPen(QPen(border, 1.5));
        painter.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 4, 4);
    }

private:
    int m_colonyId = -1;
    QPointer<GameWidget> m_game;
    QColor m_color = Qt::white;
    bool m_highlighted = false;
    QLabel* m_colorDot = nullptr;
    QLabel* m_scoreLabel = nullptr;
    QLabel* m_antsLabel = nullptr;
    QLabel* m_statusIcon = nullptr;
    QLabel* m_statusLabel = nullptr;
};
}

RtsBarWidget::RtsBarWidget(GameWidget* game, AntSimAudio* audio, QWidget* parent)
    : QWidget(parent)
{
    setObjectName("RtsBarWidget");
    setFixedWidth(220);

    setStyleSheet(
        "#RtsBarWidget {"
        "    background-color: #171b20;"
        "    color: #d7dde8;"
        "}"
        "QLabel {"
        "    color: #d7dde8;"
        "    font-size: 12px;"
        "}"
        "QPushButton {"
        "    background-color: #2a323b;"
        "    color: #d7dde8;"
        "    border: 1px solid #4a5663;"
        "    border-radius: 4px;"
        "    padding: 4px;"
        "    min-height: 22px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #34404b;"
        "}"
        "QPushButton:checked {"
        "    background-color: #536879;"
        "    border: 1px solid #8fa9bd;"
        "}"
        );

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(6);

    // ---------------------------------------------------------------------
    // Minimap direkt oben, ohne Frame und ohne Titel
    // ---------------------------------------------------------------------

    auto* miniMap = new MiniMapWidget(game, this);
    miniMap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    miniMap->setMinimumSize(0, 90);
    miniMap->setFixedHeight(120);

    root->addWidget(miniMap);

    // ---------------------------------------------------------------------
    // Kolonien + Spinnen
    // ---------------------------------------------------------------------

    auto* colonyScrollArea = new QScrollArea(this);
    colonyScrollArea->setObjectName("ColonyScrollArea");
    colonyScrollArea->viewport()->setObjectName("ColonyScrollViewport");
    colonyScrollArea->setFrameShape(QFrame::NoFrame);
    colonyScrollArea->setWidgetResizable(true);
    colonyScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    colonyScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    colonyScrollArea->setFocusPolicy(Qt::NoFocus);
    colonyScrollArea->setMinimumHeight(48);
    colonyScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    colonyScrollArea->setStyleSheet(
        "#ColonyScrollArea, #ColonyScrollViewport, #ColonyListWidget {"
        "    background-color: #171b20; border: none;"
        "}"
        );
    colonyScrollArea->verticalScrollBar()->setStyleSheet(
        "QScrollBar:vertical {"
        "    background-color: #171b20; border: none; width: 10px; margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background-color: #4a5663; border: none;"
        "    min-height: 24px; border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical:hover { background-color: #657585; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    background: none; border: none; height: 0;"
        "}"
        "QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical {"
        "    background: none; border: none; width: 0; height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "    background-color: #171b20; border: none;"
        "}"
        );
    colonyScrollArea->viewport()->setAutoFillBackground(false);

    auto* colonyWidget = new QWidget(colonyScrollArea);
    colonyWidget->setObjectName("ColonyListWidget");
    colonyWidget->setAutoFillBackground(false);
    auto* colonyLayout = new QVBoxLayout(colonyWidget);
    colonyLayout->setContentsMargins(2, 0, 2, 0);
    colonyLayout->setSpacing(3);
    colonyLayout->setAlignment(Qt::AlignTop);
    colonyLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    colonyScrollArea->setWidget(colonyWidget);
    root->addWidget(colonyScrollArea, 1);

    auto* spiderRow = new QWidget(colonyWidget);
    auto* spiderLayout = new QHBoxLayout(spiderRow);
    spiderLayout->setContentsMargins(4, 0, 4, 0);
    spiderLayout->setSpacing(6);

    auto* spiderIcon = new QLabel(QStringLiteral("🕷"), spiderRow);
    spiderIcon->setFixedWidth(18);
    spiderIcon->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto* spiderValueLabel = new QLabel(spiderRow);
    spiderValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    spiderValueLabel->setMinimumWidth(42);

    spiderLayout->addWidget(spiderIcon);
    spiderLayout->addStretch();
    spiderLayout->addWidget(spiderValueLabel);
    spiderRow->hide();
    colonyLayout->addWidget(spiderRow);

    auto colonyRows = std::make_shared<QVector<ColonyRowWidget*>>();
    auto rebuildStatsList = [game, colonyWidget, colonyLayout, colonyRows,
                             spiderRow, spiderValueLabel]() {

        if (!game || !game->world().hasMap()) {
            for (ColonyRowWidget* row : *colonyRows) {
                row->hide();
            }
            spiderRow->hide();
            return;
        }

        const GameWorld& world = game->world();

        while (colonyRows->size() > world.colonyCount()) {
            delete colonyRows->takeLast();
        }

        while (colonyRows->size() < world.colonyCount()) {
            const int colonyId = colonyRows->size();
            auto* row = new ColonyRowWidget(colonyId, game, colonyWidget);
            colonyRows->push_back(row);
            colonyLayout->addWidget(row);
        }

        QVector<ColonyDisplayRow> rows;
        rows.reserve(world.colonyCount());

        for (int i = 0; i < world.colonyCount(); ++i) {
            ColonyDisplayRow row;
            row.colonyId = i;
            row.antCount = world.antCountForColony(i);
            row.storedFood = world.colonyStoredFood(i);
            row.score = world.colonyScore(i);
            row.dead = world.colonyIsDead(i);
            row.destroyed = world.colonyIsDestroyed(i);
            row.nestHealth = world.colonyNestHealth(i);
            row.deathTimeSeconds = world.colonyDeathTimeSeconds(i);
            row.color = world.colonyColor(i);

            rows.push_back(row);
        }

        std::sort(rows.begin(), rows.end(), [](const ColonyDisplayRow& a, const ColonyDisplayRow& b) {
            if (a.dead != b.dead) {
                return !a.dead; // Lebende zuerst
            }

            if (!a.dead && !b.dead) {
                if (a.score != b.score) {
                    return a.score > b.score;
                }

                if (a.antCount != b.antCount) {
                    return a.antCount > b.antCount;
                }

                if (a.storedFood != b.storedFood) {
                    return a.storedFood > b.storedFood;
                }

                return a.colonyId < b.colonyId;
            }

            // Tote Kolonien: wer länger überlebt hat, steht weiter oben.
            if (a.deathTimeSeconds != b.deathTimeSeconds) {
                return a.deathTimeSeconds > b.deathTimeSeconds;
            }

            return a.colonyId < b.colonyId;
        });

        for (int rank = 0; rank < rows.size(); ++rank) {
            const ColonyDisplayRow& rowData = rows[rank];
            ColonyRowWidget* row = colonyRows->at(rowData.colonyId);
            row->updateData(rowData, rank + 1);
            row->show();

            colonyLayout->removeWidget(row);
            colonyLayout->insertWidget(rank, row);
        }

        const int spiders = world.spiderCount();
        if (spiders > 0) {
            spiderValueLabel->setText(QString::number(spiders));
            spiderRow->show();
        } else {
            spiderRow->hide();
        }

        colonyLayout->removeWidget(spiderRow);
        colonyLayout->addWidget(spiderRow);
    };

    // ---------------------------------------------------------------------
    // Werkzeugauswahl
    // ---------------------------------------------------------------------

    auto* toolGrid = new QGridLayout();
    toolGrid->setContentsMargins(0, 2, 0, 0);
    toolGrid->setSpacing(4);

    auto* toolGroup = new QButtonGroup(this);
    toolGroup->setExclusive(true);

    auto* pointerButton = IconFactory::makeButton(
        IconFactory::Icon::Pointer,
        this,
        true
        );

    auto* magnifierButton = IconFactory::makeButton(
        IconFactory::Icon::Magnifier,
        this,
        true
        );

    auto* foodButton = IconFactory::makeButton(
        IconFactory::Icon::Food,
        this,
        true
        );

    auto* pheromoneButton = IconFactory::makeButton(
        IconFactory::Icon::Pheromone,
        this,
        true
        );

    auto* dynamiteButton = IconFactory::makeButton(
        IconFactory::Icon::Dynamite,
        this,
        true
        );

    auto* flowerButton = IconFactory::makeButton(
        IconFactory::Icon::Flower,
        this,
        true
        );

    auto* grassButton = IconFactory::makeButton(
        IconFactory::Icon::Grass,
        this,
        true
        );

    auto* rockButton = IconFactory::makeButton(
        IconFactory::Icon::Rock,
        this,
        true
        );

    pointerButton->setChecked(true);

    foodButton->setCheckable(true);
    foodButton->setToolTip("Food");
    dynamiteButton->setToolTip("TNT platzieren");
    flowerButton->setToolTip("Blume setzen");
    grassButton->setToolTip("Rasen platzieren");
    rockButton->setToolTip("Fels platzieren");

    toolGroup->addButton(pointerButton);
    toolGroup->addButton(magnifierButton);
    toolGroup->addButton(foodButton);
    toolGroup->addButton(pheromoneButton);
    toolGroup->addButton(dynamiteButton);
    toolGroup->addButton(flowerButton);
    toolGroup->addButton(grassButton);
    toolGroup->addButton(rockButton);

    toolGrid->addWidget(pointerButton, 0, 0);
    toolGrid->addWidget(magnifierButton, 0, 1);
    toolGrid->addWidget(foodButton, 1, 0);
    toolGrid->addWidget(pheromoneButton, 1, 1);
    toolGrid->addWidget(dynamiteButton, 2, 0);
    toolGrid->addWidget(flowerButton, 2, 1);
    toolGrid->addWidget(grassButton, 3, 0);
    toolGrid->addWidget(rockButton, 3, 1);
    root->addSpacing(4);
    root->addLayout(toolGrid);

    connect(pointerButton, &QPushButton::clicked, this, [game]() {
        if (game) {
            game->setInteractionTool(GameWidget::InteractionTool::Pointer);
        }
    });

    connect(magnifierButton, &QPushButton::clicked, this, [game]() {
        if (game) {
            game->setInteractionTool(GameWidget::InteractionTool::Magnifier);
        }
    });

    connect(foodButton, &QPushButton::clicked, this, [game]() {
        if (game) {
            game->setInteractionTool(GameWidget::InteractionTool::Food);
        }
    });

    connect(pheromoneButton, &QPushButton::clicked, this, [game]() {
        if (game) {
            game->setInteractionTool(GameWidget::InteractionTool::Pheromone);
        }
    });

    connect(dynamiteButton, &QPushButton::clicked, this, [game]() {
        if (game) {
            game->setInteractionTool(GameWidget::InteractionTool::Dynamite);
        }
    });

    connect(flowerButton, &QPushButton::clicked, this, [game]() {
        if (game) {
            game->setInteractionTool(GameWidget::InteractionTool::Flower);
        }
    });

    connect(grassButton, &QPushButton::clicked, this, [game]() {
        if (game) {
            game->setInteractionTool(GameWidget::InteractionTool::Grass);
        }
    });

    connect(rockButton, &QPushButton::clicked, this, [game]() {
        if (game) {
            game->setInteractionTool(GameWidget::InteractionTool::Rock);
        }
    });

    if (game) {
        connect(game, &GameWidget::interactionToolChanged, this,
                [pointerButton, magnifierButton, foodButton, pheromoneButton, dynamiteButton, flowerButton, grassButton, rockButton](GameWidget::InteractionTool tool) {
                    pointerButton->setChecked(tool == GameWidget::InteractionTool::Pointer);
                    magnifierButton->setChecked(tool == GameWidget::InteractionTool::Magnifier);
                    foodButton->setChecked(tool == GameWidget::InteractionTool::Food);
                    pheromoneButton->setChecked(tool == GameWidget::InteractionTool::Pheromone);
                    dynamiteButton->setChecked(tool == GameWidget::InteractionTool::Dynamite);
                    flowerButton->setChecked(tool == GameWidget::InteractionTool::Flower);
                    grassButton->setChecked(tool == GameWidget::InteractionTool::Grass);
                    rockButton->setChecked(tool == GameWidget::InteractionTool::Rock);
                }
                );
    }

    // ---------------------------------------------------------------------
    // Unten: Zeit + kompakte Simulationssteuerung
    // ---------------------------------------------------------------------

    auto* bottomWidget = new QWidget(this);
    auto* bottomLayout = new QVBoxLayout(bottomWidget);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(4);

    auto* statusRow = new QWidget(bottomWidget);
    auto* statusLayout = new QHBoxLayout(statusRow);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(4);

    auto* speedLabel = new QLabel(QStringLiteral("1×"), statusRow);
    speedLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto* timeLabel = new QLabel(QStringLiteral("00:00"), statusRow);
    timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    statusLayout->addWidget(speedLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(timeLabel);

    bottomLayout->addWidget(statusRow);

    auto* speedRow = new QWidget(bottomWidget);
    auto* speedLayout = new QHBoxLayout(speedRow);
    speedLayout->setContentsMargins(0, 0, 0, 0);
    speedLayout->setSpacing(4);

    auto* speedGroup = new QButtonGroup(this);
    speedGroup->setExclusive(true);

    auto* pauseButton = IconFactory::makeButton(
        IconFactory::Icon::Pause,
        this,
        true,
        QSize(42, 30),
        QSize(22, 22)
        );

    auto* speed1Button = IconFactory::makeButton(
        IconFactory::Icon::Speed1,
        this,
        true,
        QSize(42, 30),
        QSize(22, 22)
        );

    auto* speed2Button = IconFactory::makeButton(
        IconFactory::Icon::Speed2,
        this,
        true,
        QSize(42, 30),
        QSize(22, 22)
        );

    speed1Button->setChecked(true);

    speedGroup->addButton(pauseButton);
    speedGroup->addButton(speed1Button);
    speedGroup->addButton(speed2Button);

    speedLayout->addWidget(pauseButton);
    speedLayout->addWidget(speed1Button);
    speedLayout->addWidget(speed2Button);

    bottomLayout->addWidget(speedRow);

    auto* audioRow = new QWidget(bottomWidget);
    auto* audioLayout = new QHBoxLayout(audioRow);
    audioLayout->setContentsMargins(0, 0, 0, 0);
    audioLayout->setSpacing(4);

    const bool audioAvailable = audio && audio->isAvailable();
    const bool musicAvailable = audio && audio->musicAvailable();

    auto* soundButton = new QPushButton(QStringLiteral("SFX"), audioRow);
    soundButton->setCheckable(true);
    soundButton->setChecked(audioAvailable && audio->soundEffectsEnabled());
    soundButton->setToolTip(QStringLiteral("Soundeffekte ein-/ausschalten"));

    soundButton->setEnabled(audioAvailable);

    if (!audioAvailable) {
        soundButton->setToolTip(QStringLiteral("Audio-Backend nicht verfügbar"));
    }

    auto* musicButton = new QPushButton(QStringLiteral("Music"), audioRow);
    musicButton->setCheckable(true);
    musicButton->setChecked(musicAvailable && audio->musicEnabled());
    musicButton->setEnabled(musicAvailable);
    musicButton->setToolTip(
        musicAvailable
            ? QStringLiteral("Musik ein-/ausschalten")
            : QStringLiteral("Kein music-Ordner mit WAV- oder OGG-Dateien gefunden")
        );

    if (!audioAvailable) {
        musicButton->setToolTip(QStringLiteral("Audio-Backend nicht verfügbar"));
    }

    audioLayout->addWidget(soundButton, 1);
    audioLayout->addWidget(musicButton, 1);
    bottomLayout->addWidget(audioRow);

    root->addWidget(bottomWidget);

    connect(pauseButton, &QPushButton::clicked, this, [game]() {
        if (game) {
            game->setSimulationSpeed(0.0);
        }
    });

    connect(speed1Button, &QPushButton::clicked, this, [game]() {
        if (game) {
            game->setSimulationSpeed(1.0);
        }
    });

    connect(speed2Button, &QPushButton::clicked, this, [game]() {
        if (game) {
            game->setSimulationSpeed(2.0);
        }
    });

    if (audio) {
        connect(soundButton, &QPushButton::toggled,
                audio, &AntSimAudio::setSoundEffectsEnabled);
        connect(audio, &AntSimAudio::soundEffectsEnabledChanged,
                soundButton, &QPushButton::setChecked);
        connect(musicButton, &QPushButton::toggled,
                audio, &AntSimAudio::setMusicEnabled);
        connect(audio, &AntSimAudio::musicEnabledChanged,
                musicButton, &QPushButton::setChecked);
    }

    // ---------------------------------------------------------------------
    // Signalverbindungen
    // ---------------------------------------------------------------------

    if (game) {
        connect(game, &GameWidget::worldChanged, this, [rebuildStatsList]() mutable {
            rebuildStatsList();
        });

        connect(game, &GameWidget::worldStatsChanged, this, [rebuildStatsList]() mutable {
            rebuildStatsList();
        });

        connect(game, &GameWidget::simulationTimeChanged, this, [timeLabel](double seconds) {
            timeLabel->setText(formatTime(seconds));
        });

        connect(game, &GameWidget::simulationSpeedChanged, this,
                [speedLabel, pauseButton, speed1Button, speed2Button](double speed) {
                    if (speed <= 0.0) {
                        speedLabel->setText(QStringLiteral("⏸"));
                        pauseButton->setChecked(true);
                    } else if (std::abs(speed - 2.0) < 0.0001) {
                        speedLabel->setText(QStringLiteral("2×"));
                        speed2Button->setChecked(true);
                    } else {
                        speedLabel->setText(QStringLiteral("1×"));
                        speed1Button->setChecked(true);
                    }
                }
                );
    }

    rebuildStatsList();
}
