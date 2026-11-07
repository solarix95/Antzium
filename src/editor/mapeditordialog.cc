
#include <QButtonGroup>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

#include "filedialogutils.h"
#include "mapeditordialog.h"
#include "mapeditorcanvas.h"
#include "iconfactory.h"

MapEditorDialog::MapEditorDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("🗺");
    setAttribute(Qt::WA_DeleteOnClose, true);
    resize(1100, 720);

    createUi();
}

void MapEditorDialog::createUi()
{
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(6);

    m_canvas = new MapEditorCanvas(this);

    auto* toolsWidget = new QWidget(this);
    toolsWidget->setFixedWidth(150);

    auto* tools = new QVBoxLayout(toolsWidget);
    tools->setContentsMargins(4, 4, 4, 4);
    tools->setSpacing(6);

    auto makeButton = [this](IconFactory::Icon icon, bool checkable = true) {
        return IconFactory::makeButton(
            icon,
            this,
            checkable,
            QSize(42, 34),
            QSize(25, 25)
            );
    };

    setStyleSheet(
        "QDialog {"
        "    background-color: #171b20;"
        "}"
        "QPushButton {"
        "    background-color: #2a323b;"
        "    color: #f0f3f7;"
        "    border: 1px solid #4a5663;"
        "    border-radius: 4px;"
        "    padding: 4px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #34404b;"
        "}"
        "QPushButton:checked {"
        "    background-color: #536879;"
        "    border: 1px solid #9bb6ca;"
        "}"
        );

    // ------------------------------------------------------------
    // Größe: klein / mittel / groß, nur Symbole
    // ------------------------------------------------------------

    auto* sizeGrid = new QGridLayout();
    sizeGrid->setSpacing(4);

    auto* sizeGroup = new QButtonGroup(this);
    sizeGroup->setExclusive(true);

    auto* smallButton = makeButton(IconFactory::Icon::SizeSmall);
    auto* mediumButton = makeButton(IconFactory::Icon::SizeMedium);
    auto* largeButton = makeButton(IconFactory::Icon::SizeLarge);

    mediumButton->setChecked(true);

    sizeGroup->addButton(smallButton);
    sizeGroup->addButton(mediumButton);
    sizeGroup->addButton(largeButton);

    sizeGrid->addWidget(smallButton, 0, 0);
    sizeGrid->addWidget(mediumButton, 0, 1);
    sizeGrid->addWidget(largeButton, 0, 2);

    connect(smallButton, &QPushButton::clicked, this, &MapEditorDialog::setMapSizeSmall);
    connect(mediumButton, &QPushButton::clicked, this, &MapEditorDialog::setMapSizeMedium);
    connect(largeButton, &QPushButton::clicked, this, &MapEditorDialog::setMapSizeLarge);

    tools->addLayout(sizeGrid);

    // ------------------------------------------------------------
    // Malwerkzeuge
    // ------------------------------------------------------------

    auto* toolGrid = new QGridLayout();
    toolGrid->setSpacing(4);

    auto* toolGroup = new QButtonGroup(this);
    toolGroup->setExclusive(true);

    struct ToolButton
    {
        IconFactory::Icon icon;
        MapEditorCanvas::Tool tool;
    };

    const QVector<ToolButton> toolButtons = {
        { IconFactory::Icon::Ground,        MapEditorCanvas::Tool::Ground },
        { IconFactory::Icon::Grass,         MapEditorCanvas::Tool::Grass },
        { IconFactory::Icon::Rock,          MapEditorCanvas::Tool::Rock },
        { IconFactory::Icon::Water,         MapEditorCanvas::Tool::Water },
        { IconFactory::Icon::ShallowWater,  MapEditorCanvas::Tool::ShallowWater },
        { IconFactory::Icon::Food,          MapEditorCanvas::Tool::Food },
        { IconFactory::Icon::Flower,        MapEditorCanvas::Tool::Flower },
        { IconFactory::Icon::Spider,        MapEditorCanvas::Tool::Spider },
        { IconFactory::Icon::SpiderHole,    MapEditorCanvas::Tool::SpiderHole },
        { IconFactory::Icon::ColonyRed,     MapEditorCanvas::Tool::ColonyRed },
        { IconFactory::Icon::ColonyMagenta, MapEditorCanvas::Tool::ColonyMagenta },
        { IconFactory::Icon::ColonyCoral,   MapEditorCanvas::Tool::ColonyCoral },
        { IconFactory::Icon::QueenRed,      MapEditorCanvas::Tool::QueenRed },
        { IconFactory::Icon::QueenMagenta,  MapEditorCanvas::Tool::QueenMagenta },
        { IconFactory::Icon::QueenCoral,    MapEditorCanvas::Tool::QueenCoral }
    };

    for (int i = 0; i < toolButtons.size(); ++i) {
        const ToolButton item = toolButtons[i];

        auto* button = makeButton(item.icon);

        if (i == 0) {
            button->setChecked(true);
        }

        toolGroup->addButton(button);
        toolGrid->addWidget(button, i / 3, i % 3);

        connect(button, &QPushButton::clicked, this, [this, item]() {
            m_canvas->setTool(item.tool);
        });
    }

    tools->addLayout(toolGrid);

    // ------------------------------------------------------------
    // Aktionen: Zufall, Laden, Speichern, Los
    // ------------------------------------------------------------

    auto* actionGrid = new QGridLayout();
    actionGrid->setSpacing(4);

    auto* randomButton = makeButton(IconFactory::Icon::Random, false);
    auto* loadButton = makeButton(IconFactory::Icon::Open, false);
    auto* saveButton = makeButton(IconFactory::Icon::Save, false);
    auto* goButton = makeButton(IconFactory::Icon::Play, false);

    randomButton->setMinimumSize(42, 34);
    loadButton->setMinimumSize(42, 34);
    saveButton->setMinimumSize(42, 34);
    goButton->setMinimumSize(42, 34);

    actionGrid->addWidget(randomButton, 0, 0);
    actionGrid->addWidget(loadButton, 0, 1);
    actionGrid->addWidget(saveButton, 1, 0);
    actionGrid->addWidget(goButton, 1, 1);

    connect(randomButton, &QPushButton::clicked, m_canvas, &MapEditorCanvas::randomize);
    connect(loadButton, &QPushButton::clicked, this, &MapEditorDialog::loadPng);
    connect(saveButton, &QPushButton::clicked, this, &MapEditorDialog::savePng);

    connect(goButton, &QPushButton::clicked, this, [this]() {
        emit startScenarioRequested(m_canvas->image());
    });

    tools->addLayout(actionGrid);
    tools->addStretch();

    root->addWidget(m_canvas, 1);
    root->addWidget(toolsWidget, 0);
}

void MapEditorDialog::setMapSizeSmall()
{
    m_canvas->createBlankMap(QSize(128, 80));
}

void MapEditorDialog::setMapSizeMedium()
{
    m_canvas->createBlankMap(QSize(256, 144));
}

void MapEditorDialog::setMapSizeLarge()
{
    m_canvas->createBlankMap(QSize(512, 288));
}

void MapEditorDialog::savePng()
{
    const QString fileName = FileDialogUtils::getFileName(
        this,
        QString(),
        QStringLiteral("PNG (*.png)"),
        QFileDialog::AcceptSave
        );

    if (fileName.isEmpty()) {
        return;
    }

    QString finalName = fileName;

    if (!finalName.endsWith(".png", Qt::CaseInsensitive)) {
        finalName += ".png";
    }

    m_canvas->image().save(finalName, "PNG");
}

void MapEditorDialog::loadPng()
{
    const QString fileName = FileDialogUtils::getFileName(
        this,
        QString(),
        QStringLiteral("PNG (*.png)"),
        QFileDialog::AcceptOpen
        );

    if (fileName.isEmpty()) {
        return;
    }

    QImage image;

    if (!image.load(fileName)) {
        return;
    }

    m_canvas->setImage(image);
}
