#ifndef VPET_TTS_AUDIO_PLAYER_H
#define VPET_TTS_AUDIO_PLAYER_H

#include <QObject>
#include <QString>

class QSoundEffect;

namespace vpet
{

/**
 * @brief TTS 音频播放器
 *
 * 使用 QSoundEffect 播放 WAV 音频文件，
 * 支持低延迟播放与播放完成信号通知。
 */
class TtsAudioPlayer : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param[in] parent 父对象
     */
    explicit TtsAudioPlayer(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~TtsAudioPlayer() override;

    /**
     * @brief 播放音频文件
     * @param[in] filePath WAV 文件路径，不得为空
     */
    void Play(const QString &filePath);

    /**
     * @brief 停止当前播放
     */
    void Stop();

    /**
     * @brief 判断是否正在播放
     * @return 正在播放返回 true
     */
    bool IsPlaying() const;

signals:
    /**
     * @brief 播放完成信号
     */
    void PlaybackFinished();

private:
    QSoundEffect *m_soundEffect; ///< 音效播放器
};

} // namespace vpet

#endif // VPET_TTS_AUDIO_PLAYER_H
