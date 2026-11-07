#include "antsimaudio.h"

#include <QDataStream>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <functional>

#include <qtrsnd.h>
#include <qtrsndmusic.h>
#include <qtrsndsample.h>

namespace
{
constexpr int SampleRate = 22050;
constexpr double Pi = 3.14159265358979323846;

double clampSample(double value)
{
    return std::max(-1.0, std::min(1.0, value));
}

double sine(double frequency, double time)
{
    return std::sin(2.0 * Pi * frequency * time);
}

bool writeWave(
    const QString& fileName,
    double durationSeconds,
    const std::function<double(double, int)>& generator
    )
{
    const int frameCount = std::max(1, static_cast<int>(durationSeconds * SampleRate));
    const quint32 dataSize = static_cast<quint32>(frameCount * sizeof(qint16));

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "AntSimAudio: cannot create" << fileName << file.errorString();
        return false;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream.writeRawData("RIFF", 4);
    stream << quint32(36 + dataSize);
    stream.writeRawData("WAVE", 4);
    stream.writeRawData("fmt ", 4);
    stream << quint32(16);
    stream << quint16(1);
    stream << quint16(1);
    stream << quint32(SampleRate);
    stream << quint32(SampleRate * sizeof(qint16));
    stream << quint16(sizeof(qint16));
    stream << quint16(16);
    stream.writeRawData("data", 4);
    stream << dataSize;

    for (int frame = 0; frame < frameCount; ++frame) {
        const double time = static_cast<double>(frame) / SampleRate;
        const double value = clampSample(generator(time, frame));
        stream << static_cast<qint16>(value * 32767.0);
    }

    return stream.status() == QDataStream::Ok;
}

double deterministicNoise(int frame)
{
    quint32 value = static_cast<quint32>(frame) * 747796405u + 2891336453u;
    value = ((value >> ((value >> 28u) + 4u)) ^ value) * 277803737u;
    value = (value >> 22u) ^ value;
    return static_cast<double>(value & 0xffffu) / 32767.5 - 1.0;
}
}

AntSimAudio::AntSimAudio(QObject* parent)
    : QObject(parent)
    , m_samplePools(static_cast<int>(SoundEffect::Count))
{
    m_clock.start();
    m_musicPauseTimer.setSingleShot(true);
    connect(&m_musicPauseTimer, &QTimer::timeout,
            this, &AntSimAudio::playNextMusic);
}

AntSimAudio::~AntSimAudio()
{
    stopMusic();
    releaseAudioObjects();
    if (m_initialized) {
        QtrSnd::destroy();
    }
    delete m_assetDirectory;
}

bool AntSimAudio::initialize()
{
    if (m_initialized) {
        return m_available;
    }

    m_initialized = true;

    const QStringList engines = QtrSnd::availableEngines();
    if (engines.isEmpty()) {
        qWarning() << "AntSimAudio: libqtrsound has no compiled audio backend";
        return false;
    }

    const QString engine = engines.contains(QStringLiteral("sdl2"))
                               ? QStringLiteral("sdl2")
                               : engines.first();

    const QString options = QStringLiteral(
                                "\"engine\":\"%1\",\"frequency\":%2,\"channels\":2,\"chunksize\":512,\"mixChannels\":24"
                                ).arg(engine).arg(SampleRate);

    if (!QtrSnd::init(options)) {
        qWarning() << "AntSimAudio: cannot initialize libqtrsound backend" << engine;
        QtrSnd::destroy();
        return false;
    }

    if (!createSynthesizedAssets()) {
        QtrSnd::destroy();
        return false;
    }

    const QString root = m_assetDirectory->path();
    bool ok = true;
    ok &= createSamplePool(SoundEffect::FoodPlaced, root + "/place.wav", 3, 34, 45);
    ok &= createSamplePool(SoundEffect::DynamitePlaced, root + "/fuse.wav", 3, 48, 70);
    ok &= createSamplePool(SoundEffect::FlowerPlaced, root + "/flower.wav", 3, 34, 70);
    ok &= createSamplePool(SoundEffect::TerrainPainted, root + "/terrain.wav", 3, 24, 110);
    ok &= createSamplePool(SoundEffect::Burn, root + "/burn.wav", 4, 35, 100);
    ok &= createSamplePool(
        SoundEffect::Explosion,
        QStringLiteral(":/sounds/bomb-explosion.wav"),
        6,
        88,
        0
        );
    ok &= createSamplePool(
        SoundEffect::WaspKillsSpider,
        QStringLiteral(":/sounds/cartoon-splat.wav"),
        4,
        72,
        0
        );
    ok &= createSamplePool(
        SoundEffect::SpiderWinsDuel,
        QStringLiteral(":/sounds/spider-bite.wav"),
        4,
        70,
        0
        );
    ok &= createSamplePool(
        SoundEffect::ColonyDied,
        QStringLiteral(":/sounds/colony-death.wav"),
        2,
        64,
        250
        );
    ok &= createSamplePool(
        SoundEffect::ColonyFounded,
        QStringLiteral(":/sounds/colony-founded.wav"),
        2,
        62,
        250
        );
    ok &= createSamplePool(
        SoundEffect::QueenSpawned,
        QStringLiteral(":/sounds/queen-spawn.wav"),
        2,
        58,
        180
        );
    ok &= createSamplePool(
        SoundEffect::AmbientThunder1,
        QStringLiteral(":/sounds/ambient-thunder-1.wav"),
        1,
        72,
        0
        );
    ok &= createSamplePool(
        SoundEffect::AmbientThunder2,
        QStringLiteral(":/sounds/ambient-thunder-2.wav"),
        1,
        72,
        0
        );
    ok &= createSamplePool(
        SoundEffect::AmbientThunder3,
        QStringLiteral(":/sounds/ambient-thunder-3.wav"),
        1,
        72,
        0
        );
    ok &= createSamplePool(
        SoundEffect::AmbientCricket1,
        QStringLiteral(":/sounds/ambient-cricket-1.wav"),
        1,
        42,
        0
        );
    ok &= createSamplePool(
        SoundEffect::AmbientBird1,
        QStringLiteral(":/sounds/ambient-bird-1.wav"),
        1,
        42,
        0
        );
    ok &= createSamplePool(
        SoundEffect::AmbientCricket2,
        QStringLiteral(":/sounds/ambient-cricket-2.wav"),
        1,
        42,
        0
        );
    ok &= createSamplePool(
        SoundEffect::AmbientBird2,
        QStringLiteral(":/sounds/ambient-bird-2.wav"),
        1,
        42,
        0
        );

    if (!ok) {
        qWarning() << "AntSimAudio: one or more audio voices could not be created";
        releaseAudioObjects();
        QtrSnd::destroy();
        return false;
    }

    m_available = true;
    discoverMusicFiles();
    playNextMusic();
    return true;
}

bool AntSimAudio::isAvailable() const
{
    return m_available;
}

bool AntSimAudio::soundEffectsEnabled() const
{
    return m_soundEffectsEnabled;
}

bool AntSimAudio::musicAvailable() const
{
    return m_available && !m_musicFiles.isEmpty();
}

bool AntSimAudio::musicEnabled() const
{
    return m_musicEnabled;
}

void AntSimAudio::playEffect(SoundEffect effect, const QPointF& worldPosition)
{
    if (!m_available || !m_soundEffectsEnabled) {
        return;
    }

    const int index = effectIndex(effect);
    if (index < 0 || index >= m_samplePools.size()) {
        return;
    }

    SamplePool& pool = m_samplePools[index];
    if (pool.voices.isEmpty()) {
        return;
    }

    const qint64 now = m_clock.elapsed();
    if (now - pool.lastPlayMs < pool.cooldownMs) {
        return;
    }

    const int volume = attenuatedVolume(pool.volume, worldPosition);
    if (volume <= 0) {
        return;
    }

    QtrSndSample* voice = pool.voices[pool.nextVoice];
    pool.nextVoice = (pool.nextVoice + 1) % pool.voices.size();
    pool.lastPlayMs = now;

    voice->setVolume(volume);
    voice->setPan(panForPosition(worldPosition));

    if (voice->supportsPitch()) {
        const float variation = 0.94f + static_cast<float>(QRandomGenerator::global()->generateDouble()) * 0.12f;
        voice->setPitch(variation);
    }

    voice->play();
}

void AntSimAudio::setListenerRect(const QRectF& worldRect)
{
    m_listenerRect = worldRect;
}

void AntSimAudio::setSoundEffectsEnabled(bool enabled)
{
    if (m_soundEffectsEnabled == enabled) {
        return;
    }

    m_soundEffectsEnabled = enabled;
    if (!enabled) {
        for (SamplePool& pool : m_samplePools) {
            for (QtrSndSample* voice : pool.voices) {
                voice->stop();
            }
        }
    }

    emit soundEffectsEnabledChanged(enabled);
}

void AntSimAudio::setMusicEnabled(bool enabled)
{
    if (m_musicEnabled == enabled) {
        return;
    }

    m_musicEnabled = enabled;
    if (enabled) {
        playNextMusic();
    } else {
        stopMusic();
    }

    emit musicEnabledChanged(enabled);
}

void AntSimAudio::discoverMusicFiles()
{
    m_musicFiles.clear();
    m_musicQueue.clear();
    m_lastMusicFile.clear();

    const QStringList directoryCandidates = {
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("music")),
        QDir::current().filePath(QStringLiteral("music"))
    };

    QStringList visitedDirectories;
    for (const QString& candidate : directoryCandidates) {
        const QFileInfo directoryInfo(candidate);
        if (!directoryInfo.exists() || !directoryInfo.isDir()) {
            continue;
        }

        const QString directoryPath = directoryInfo.canonicalFilePath();
        if (directoryPath.isEmpty() || visitedDirectories.contains(directoryPath)) {
            continue;
        }
        visitedDirectories.push_back(directoryPath);

        const QFileInfoList entries = QDir(directoryPath).entryInfoList(
            QDir::Files | QDir::Readable,
            QDir::Name | QDir::IgnoreCase
            );

        for (const QFileInfo& entry : entries) {
            const QString suffix = entry.suffix();
            if (suffix.compare(QStringLiteral("wav"), Qt::CaseInsensitive) != 0 &&
                suffix.compare(QStringLiteral("ogg"), Qt::CaseInsensitive) != 0) {
                continue;
            }

            const QString filePath = entry.canonicalFilePath();
            if (!filePath.isEmpty() && !m_musicFiles.contains(filePath)) {
                m_musicFiles.push_back(filePath);
            }
        }

        if (!m_musicFiles.isEmpty()) {
            qInfo() << "AntSimAudio: found" << m_musicFiles.size()
                    << "music tracks in" << directoryPath;
            break;
        }
    }
}

void AntSimAudio::playNextMusic()
{
    m_musicPauseTimer.stop();

    if (!m_available || !m_musicEnabled || m_music || m_musicFiles.isEmpty()) {
        return;
    }

    while (!m_musicFiles.isEmpty()) {
        if (m_musicQueue.isEmpty()) {
            m_musicQueue = m_musicFiles;
        }

        int queueIndex = QRandomGenerator::global()->bounded(m_musicQueue.size());
        if (m_musicQueue.size() > 1 &&
            m_musicQueue.at(queueIndex) == m_lastMusicFile) {
            queueIndex = (queueIndex + 1) % m_musicQueue.size();
        }

        const QString fileName = m_musicQueue.takeAt(queueIndex);
        QtrSndMusic* music = QtrSnd::music(fileName);
        if (!music) {
            m_musicFiles.removeAll(fileName);
            continue;
        }

        m_music = music;
        connect(music, &QtrSndMusic::stateChanged, this,
                [this, music](int state) {
                    if (music != m_music ||
                        state != static_cast<int>(QtrSndMusic::EndedState)) {
                        return;
                    }

                    m_music = nullptr;
                    music->deleteLater();
                    scheduleNextMusic();
                });

        music->setLoop(false);
        music->setVolume(42);
        music->play();

        if (music->state() == QtrSndMusic::PlayingState) {
            m_lastMusicFile = fileName;
            return;
        }

        qWarning() << "AntSimAudio: cannot play music" << fileName;
        m_music = nullptr;
        disconnect(music, nullptr, this, nullptr);
        delete music;
        m_musicFiles.removeAll(fileName);
        m_musicQueue.removeAll(fileName);
    }
}

void AntSimAudio::scheduleNextMusic()
{
    if (!m_available || !m_musicEnabled || m_musicFiles.isEmpty()) {
        return;
    }

    const int pauseMs = 2000 + QRandomGenerator::global()->bounded(1001);
    m_musicPauseTimer.start(pauseMs);
}

void AntSimAudio::stopMusic()
{
    m_musicPauseTimer.stop();

    if (!m_music) {
        return;
    }

    QtrSndMusic* music = m_music;
    m_music = nullptr;
    disconnect(music, nullptr, this, nullptr);
    music->stop();
    delete music;
}

bool AntSimAudio::createSynthesizedAssets()
{
    m_assetDirectory = new QTemporaryDir(
        QDir::tempPath() + QStringLiteral("/antsim-audio-XXXXXX")
        );
    if (!m_assetDirectory->isValid()) {
        qWarning() << "AntSimAudio: cannot create temporary audio directory";
        return false;
    }

    const QString root = m_assetDirectory->path();

    const bool place = writeWave(root + "/place.wav", 0.16, [](double time, int) {
        const double envelope = std::exp(-time * 18.0);
        return envelope * (0.55 * sine(900.0, time) + 0.22 * sine(1350.0, time));
    });

    const bool fuse = writeWave(root + "/fuse.wav", 0.24, [](double time, int frame) {
        const double envelope = std::exp(-time * 13.0);
        return envelope * (0.38 * deterministicNoise(frame) + 0.3 * sine(1850.0, time));
    });

    const bool flower = writeWave(root + "/flower.wav", 0.38, [](double time, int) {
        const double envelope = std::min(1.0, time / 0.02) * std::exp(-time * 5.8);
        return envelope * (0.34 * sine(660.0, time) + 0.28 * sine(990.0, time));
    });

    const bool terrain = writeWave(root + "/terrain.wav", 0.18, [](double time, int frame) {
        const double envelope = std::exp(-time * 22.0);
        return envelope * (0.24 * deterministicNoise(frame) + 0.16 * sine(170.0, time));
    });

    const bool burn = writeWave(root + "/burn.wav", 0.55, [](double time, int frame) {
        const double envelope = std::min(1.0, time / 0.025) * std::exp(-time * 4.4);
        const double crackle = deterministicNoise(frame) * (0.35 + 0.25 * sine(37.0, time));
        return envelope * (0.34 * crackle + 0.16 * sine(95.0, time));
    });

    return place && fuse && flower && terrain && burn;
}

bool AntSimAudio::createSamplePool(
    SoundEffect effect,
    const QString& fileName,
    int voiceCount,
    int volume,
    int cooldownMs
    )
{
    const int index = effectIndex(effect);
    if (index < 0 || index >= m_samplePools.size()) {
        return false;
    }

    SamplePool& pool = m_samplePools[index];
    pool.volume = volume;
    pool.cooldownMs = cooldownMs;

    for (int i = 0; i < voiceCount; ++i) {
        QtrSndSample* voice = QtrSnd::sample(fileName);
        if (!voice) {
            return false;
        }
        pool.voices.push_back(voice);
    }

    return true;
}

void AntSimAudio::releaseAudioObjects()
{
    for (SamplePool& pool : m_samplePools) {
        qDeleteAll(pool.voices);
        pool.voices.clear();
    }

    m_available = false;
}

int AntSimAudio::effectIndex(SoundEffect effect) const
{
    return static_cast<int>(effect);
}

int AntSimAudio::panForPosition(const QPointF& worldPosition) const
{
    if (m_listenerRect.width() <= 1.0) {
        return 0;
    }

    const double normalized =
        (worldPosition.x() - m_listenerRect.center().x()) /
        (m_listenerRect.width() * 0.5);

    return static_cast<int>(std::round(std::max(-1.0, std::min(1.0, normalized)) * 100.0));
}

int AntSimAudio::attenuatedVolume(int baseVolume, const QPointF& worldPosition) const
{
    if (m_listenerRect.isEmpty() || m_listenerRect.contains(worldPosition)) {
        return baseVolume;
    }

    const double dx = std::max(
        std::max(m_listenerRect.left() - worldPosition.x(), 0.0),
        worldPosition.x() - m_listenerRect.right()
        );
    const double dy = std::max(
        std::max(m_listenerRect.top() - worldPosition.y(), 0.0),
        worldPosition.y() - m_listenerRect.bottom()
        );
    const double distance = std::sqrt(dx * dx + dy * dy);
    const double audibleDistance = std::max(m_listenerRect.width(), m_listenerRect.height()) * 0.65;

    if (distance >= audibleDistance || audibleDistance <= 0.0) {
        return 0;
    }

    const double gain = 1.0 - distance / audibleDistance;
    return static_cast<int>(std::round(baseVolume * gain));
}
