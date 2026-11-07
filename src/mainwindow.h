#pragma once

#include <QMainWindow>
#include <QPointer>

class MapEditorDialog;
class GameWidget;
class AntSimAudio;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void showStartMenu();
    void showMapEditor();
    void saveBaseMapAs();
    void saveGameStateAs();
    void loadGameState();

private:
    void centerChildDialog(QWidget* dialog);
    void toggleFullscreen();

private:
    GameWidget      *m_game = nullptr;
    MapEditorDialog *m_mapEditor = nullptr;
    AntSimAudio      *m_audio = nullptr;
};
