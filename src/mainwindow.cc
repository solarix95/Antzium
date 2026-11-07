
#include <QDialog>
#include <QLabel>
#include <QFileDialog>
#include <QGridLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>
#include <QKeyEvent>

#include "mainwindow.h"
#include "audio/antsimaudio.h"
#include "gamewidget.h"
#include "rtsbarwidget.h"
#include "iconfactory.h"
#include "filedialogutils.h"
#include "editor/mapeditordialog.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    m_audio = new AntSimAudio(this);
    m_audio->initialize();

    auto* root = new QWidget(this);
    auto* layout = new QHBoxLayout(root);

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_game = new GameWidget(root);
    auto* rtsBar = new RtsBarWidget(m_game, m_audio, root);

    connect(m_game, &GameWidget::soundEffectRequested,
            m_audio, &AntSimAudio::playEffect);
    connect(m_game, &GameWidget::viewChanged, this, [this]() {
        if (m_audio && m_game) {
            m_audio->setListenerRect(m_game->cameraWorldRect());
        }
    });

    layout->addWidget(m_game, 1);
    layout->addWidget(rtsBar, 0);

    setCentralWidget(root);

    resize(1280, 720);
    setWindowTitle("Antzium - God of Ants");

    auto* startMenuShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    startMenuShortcut->setContext(Qt::WindowShortcut);

    connect(startMenuShortcut, &QShortcut::activated, this, &MainWindow::showStartMenu);

    m_game->startDemo();

    QTimer::singleShot(0, this, &MainWindow::showStartMenu);
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F12 && !event->isAutoRepeat()) {
        toggleFullscreen();
        event->accept();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void MainWindow::showStartMenu()
{
    auto* dialog = new QDialog(this);

    dialog->setWindowTitle(QString());
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setModal(false);

    dialog->setStyleSheet(
        "QDialog {"
        "    background-color: #171b20;"
        "}"
        "QLabel {"
        "    color: #d7dde8;"
        "    font-size: 11px;"
        "}"
        "QPushButton {"
        "    background-color: #2a323b;"
        "    color: #f0f3f7;"
        "    border: 1px solid #4a5663;"
        "    border-radius: 6px;"
        "    padding: 8px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #34404b;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #536879;"
        "}"
        );

    auto* layout = new QGridLayout(dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(8);

    auto* loadLabel = new QLabel(QStringLiteral("Laden / Neu"), dialog);
    auto* saveLabel = new QLabel(QStringLiteral("Speichern"), dialog);

    auto* openMapButton = IconFactory::makeButton(
        IconFactory::Icon::Open,
        dialog,
        false,
        QSize(58, 46),
        QSize(30, 30)
        );
    openMapButton->setToolTip(QStringLiteral("PNG-Map laden"));

    auto* loadStateButton = IconFactory::makeButton(
        IconFactory::Icon::Play,
        dialog,
        false,
        QSize(58, 46),
        QSize(30, 30)
        );
    loadStateButton->setToolTip(QStringLiteral("Spielstand laden"));

    auto* editorButton = IconFactory::makeButton(
        IconFactory::Icon::Ground,
        dialog,
        false,
        QSize(58, 46),
        QSize(30, 30)
        );
    editorButton->setToolTip(QStringLiteral("Map-Editor öffnen"));

    auto* resumeButton = IconFactory::makeButton(
        IconFactory::Icon::Pointer,
        dialog,
        false,
        QSize(58, 46),
        QSize(30, 30)
        );
    resumeButton->setToolTip(QStringLiteral("Weiterspielen"));

    auto* saveMapButton = IconFactory::makeButton(
        IconFactory::Icon::Ground,
        dialog,
        false,
        QSize(58, 46),
        QSize(30, 30)
        );
    saveMapButton->setToolTip(QStringLiteral("Als neue PNG-Basemap speichern"));

    auto* saveStateButton = IconFactory::makeButton(
        IconFactory::Icon::Save,
        dialog,
        false,
        QSize(58, 46),
        QSize(30, 30)
        );
    saveStateButton->setToolTip(QStringLiteral("Spielstand als JSON speichern"));

    layout->addWidget(loadLabel, 0, 0, 1, 4);
    layout->addWidget(openMapButton, 1, 0);
    layout->addWidget(loadStateButton, 1, 1);
    layout->addWidget(editorButton, 1, 2);
    layout->addWidget(resumeButton, 1, 3);

    layout->addWidget(saveLabel, 2, 0, 1, 4);
    layout->addWidget(saveMapButton, 3, 0);
    layout->addWidget(saveStateButton, 3, 1);

    connect(openMapButton, &QPushButton::clicked, dialog, [this, dialog]() {
        const QString fileName = FileDialogUtils::getFileName(
            dialog,
            QStringLiteral("PNG-Map laden"),
            QStringLiteral("PNG (*.png)"),
            QFileDialog::AcceptOpen
            );

        if (!fileName.isEmpty() && m_game) {
            m_game->loadMapFromFile(fileName);
            m_game->setFocus();
            dialog->close();
        }
    });

    connect(loadStateButton, &QPushButton::clicked, dialog, [this, dialog]() {
        dialog->close();
        loadGameState();
    });

    connect(editorButton, &QPushButton::clicked, dialog, [this, dialog]() {
        dialog->close();
        showMapEditor();
    });

    connect(resumeButton, &QPushButton::clicked, dialog, [this, dialog]() {
        if (m_game) {
            m_game->setFocus();
        }

        dialog->close();
    });

    connect(saveMapButton, &QPushButton::clicked, dialog, [this, dialog]() {
        dialog->close();
        saveBaseMapAs();
    });

    connect(saveStateButton, &QPushButton::clicked, dialog, [this, dialog]() {
        dialog->close();
        saveGameStateAs();
    });

    dialog->adjustSize();
    centerChildDialog(dialog);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void MainWindow::showMapEditor()
{
    if (m_mapEditor) {
        m_mapEditor->raise();
        m_mapEditor->activateWindow();
        return;
    }

    m_mapEditor = new MapEditorDialog(this);

    connect(m_mapEditor, &MapEditorDialog::startScenarioRequested, this, [this](const QImage& image) {
        if (m_game) {
            m_game->loadMapFromImage(image);
            m_game->setFocus();
        }
    });

    connect(m_mapEditor, &QObject::destroyed, this, [this](QObject *o) {
        if (o == m_mapEditor)
            m_mapEditor = nullptr;
    });

    m_mapEditor->show();
}

void MainWindow::saveBaseMapAs()
{
    if (!m_game) {
        return;
    }

    QString fileName = FileDialogUtils::getFileName(
        this,
        QStringLiteral("Als neue PNG-Basemap speichern"),
        QStringLiteral("PNG (*.png)"),
        QFileDialog::AcceptSave
        );

    if (fileName.isEmpty()) {
        return;
    }

    if (!fileName.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) {
        fileName += QStringLiteral(".png");
    }

    if (!m_game->saveBaseMapPng(fileName)) {
        QMessageBox::warning(
            this,
            QStringLiteral("Speichern fehlgeschlagen"),
            QStringLiteral("Die PNG-Basemap konnte nicht gespeichert werden.")
            );
    }
}

void MainWindow::saveGameStateAs()
{
    if (!m_game) {
        return;
    }

    QString fileName = FileDialogUtils::getFileName(
        this,
        QStringLiteral("Spielstand als JSON speichern"),
        QStringLiteral("JSON (*.json)"),
        QFileDialog::AcceptSave
        );

    if (fileName.isEmpty()) {
        return;
    }

    if (!fileName.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
        fileName += QStringLiteral(".json");
    }

    if (!m_game->saveStateJson(fileName)) {
        QMessageBox::warning(
            this,
            QStringLiteral("Speichern fehlgeschlagen"),
            QStringLiteral("Der Spielstand konnte nicht gespeichert werden.")
            );
    }
}

void MainWindow::loadGameState()
{
    if (!m_game) {
        return;
    }

    const QString fileName = FileDialogUtils::getFileName(
        this,
        QStringLiteral("Spielstand laden"),
        QStringLiteral("JSON (*.json)"),
        QFileDialog::AcceptOpen
        );

    if (fileName.isEmpty()) {
        return;
    }

    if (!m_game->loadStateJson(fileName)) {
        QMessageBox::warning(
            this,
            QStringLiteral("Laden fehlgeschlagen"),
            QStringLiteral("Der Spielstand konnte nicht geladen werden.")
            );
        return;
    }

    m_game->setFocus();
}

void MainWindow::centerChildDialog(QWidget* dialog)
{
    const QPoint center = geometry().center();
    const QSize size = dialog->size();

    dialog->move(center.x() - size.width() / 2, center.y() - size.height() / 2);
}
