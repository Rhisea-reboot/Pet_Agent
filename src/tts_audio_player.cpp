#include "vpet/tts_audio_player.h"

#include <QDebug>
#include <QFileInfo>
#include <QSoundEffect>
#include <QUrl>

namespace vpet
{

TtsAudioPlayer::TtsAudioPlayer(QObject *parent)
    : QObject(parent)
    , m_soundEffect(nullptr)
{
    m_soundEffect = new QSoundEffect(this);

    connect(m_soundEffect, &QSoundEffect::playingChanged,
            this, [this]()
    {
        if (!m_soundEffect->isPlaying())
        {
            emit PlaybackFinished();
        }
    });
}

TtsAudioPlayer::~TtsAudioPlayer()
{
    Stop();
}

void TtsAudioPlayer::Play(const QString &filePath)
{
    qDebug() << "[TTS] TtsAudioPlayer::Play, file:" << filePath;

    // 检查参数有效性
    if (filePath.isEmpty())
    {
        qDebug() << "[TTS]   FAILED - empty filePath";
        return;
    }

    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists() || !fileInfo.isFile())
    {
        qDebug() << "[TTS]   FAILED - file not found or not a file. exists:" << fileInfo.exists();
        return;
    }

    qDebug() << "[TTS]   file size:" << fileInfo.size() << "bytes";

    Stop();

    m_soundEffect->setSource(QUrl::fromLocalFile(fileInfo.absoluteFilePath()));
    m_soundEffect->setVolume(1.0f);
    m_soundEffect->play();

    qDebug() << "[TTS]   playback started";
}

void TtsAudioPlayer::Stop()
{
    if (m_soundEffect->isPlaying())
    {
        m_soundEffect->stop();
    }
}

bool TtsAudioPlayer::IsPlaying() const
{
    return m_soundEffect->isPlaying();
}

} // namespace vpet
