#pragma once

#include <QWidget>

class GameWidget;
class AntSimAudio;

class RtsBarWidget : public QWidget
{
public:
    explicit RtsBarWidget(GameWidget* game, AntSimAudio* audio, QWidget* parent = nullptr);
};
