#ifndef MAPEDITORDIALOG_H
#define MAPEDITORDIALOG_H

#include <QDialog>
#include <QImage>

class MapEditorCanvas;

class MapEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MapEditorDialog(QWidget* parent = nullptr);

signals:
    void startScenarioRequested(const QImage& image);

private:
    void createUi();
    void setMapSizeSmall();
    void setMapSizeMedium();
    void setMapSizeLarge();

    void savePng();
    void loadPng();

private:
    MapEditorCanvas* m_canvas = nullptr;
};

#endif // MAPEDITORDIALOG_H
