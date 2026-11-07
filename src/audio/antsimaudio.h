#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include "soundevent.h"

class QTemporaryDir;
class QtrSndMusic;
class QtrSndSample;

class AntSimAudio : public QObject
{
    Q_OBJECT

public:
    explicit AntSimAudio(QObject* parent = nullptr);
    ~AntSimAudio() override;

    bool initialize();
    bool isAvailable() const;
    bool soundEffectsEnabled() const;
    bool musicAvailable() const;
    bool musicEnabled() const;

public slots:
    void playEffect(SoundEffect effect, const QPointF& worldPosition);
    void setListenerRect(const QRectF& worldRect);
    void setSoundEffectsEnabled(bool enabled);
    void setMusicEnabled(bool enabled);

signals:
    void soundEffectsEnabledChanged(bool enabled);
    void musicEnabledChanged(bool enabled);

private:
    struct SamplePool
    {
        QVector<QtrSndSample*> voices;
        int nextVoice = 0;
        int volume = 100;
        int cooldownMs = 0;
        qint64 lastPlayMs = -100000;
    };

    bool createSynthesizedAssets();
    bool createSamplePool(SoundEffect effect, const QString& fileName, int voiceCount, int volume, int cooldownMs);
    void discoverMusicFiles();
    void playNextMusic();
    void scheduleNextMusic();
    void stopMusic();
    void releaseAudioObjects();
    int effectIndex(SoundEffect effect) const;
    int panForPosition(const QPointF& worldPosition) const;
    int attenuatedVolume(int baseVolume, const QPointF& worldPosition) const;

    bool m_initialized = false;
    bool m_available = false;
    bool m_soundEffectsEnabled = true;
    bool m_musicEnabled = true;

    QRectF m_listenerRect;
    QElapsedTimer m_clock;
    QTimer m_musicPauseTimer;
    QTemporaryDir* m_assetDirectory = nullptr;
    QtrSndMusic* m_music = nullptr;
    QStringList m_musicFiles;
    QStringList m_musicQueue;
    QString m_lastMusicFile;
    QVector<SamplePool> m_samplePools;
};
