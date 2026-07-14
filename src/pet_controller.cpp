#include "vpet/pet_controller.h"
#include "vpet/say_dialog.h"
#include "vpet/tts_client.h"
#include "vpet/tts_audio_player.h"

#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QRandomGenerator>
#include <QScreen>

namespace vpet
{

namespace
{

constexpr int UPDATE_INTERVAL_MS = 16;                  ///< 主循环更新间隔，约 60 FPS
constexpr int BUBBLE_DURATION_MS = 2000;                ///< 气泡默认显示时长
constexpr int IDLE_TRIGGER_INTERVAL_MS = 3000;          ///< Idle 随机触发间隔
constexpr int WALK_SPEED_PIXELS_PER_SECOND = 80;        ///< 默认步行速度
constexpr int DRAG_MOVE_THRESHOLD_PIXELS = 5;           ///< 拖拽与点击的位移阈值

} // anonymous namespace

PetController::PetController(const QString &animationBasePath, QObject *parent)
    : QObject(parent)
    , m_resourceManager(animationBasePath)
    , m_stateMachine(m_resourceManager)
    , m_bubbleMessage()
    , m_hitRegion()
    , m_updateTimer(nullptr)
    , m_position(100, 100)
    , m_dragStartGlobalPos()
    , m_windowStartPos()
    , m_screenBounds()
    , m_walkSpeedPixelsPerSecond(WALK_SPEED_PIXELS_PER_SECOND)
    , m_isDragging(false)
    , m_dragMoved(false)
    , m_pendingHitType(HIT_TYPE::NONE)
    , m_lastState(PET_STATE::IDLE)
    , m_frameSize(100, 100)
    , m_ttsClient(nullptr)
    , m_ttsAudioPlayer(nullptr)
    , m_tempDir()
    , m_currentSayText()
    , m_sayTextShown(false)
    , m_synthesisCounter(0)
{
}

bool PetController::Initialize()
{
    if (!m_resourceManager.LoadAll())
    {
        return false;
    }

    m_stateMachine.Initialize();
    m_lastState = m_stateMachine.GetCurrentState();

    // 初始化 TTS 客户端
    m_ttsClient = new TtsClient(this);

    // 多级路径查找配置文件（与 TtsServerManager::FindConfigFile 逻辑一致）
    const QString exeDir = QCoreApplication::applicationDirPath();
    qDebug() << "[TTS] PetController init - exeDir:" << exeDir;
    qDebug() << "[TTS] PetController init - cwd:" << QDir::currentPath();

    const QStringList candidatePaths =
    {
        exeDir + QStringLiteral("/tts_config.json"),
        QDir::currentPath() + QStringLiteral("/tts_config.json"),
        exeDir + QStringLiteral("/../tts_config.json"),
        exeDir + QStringLiteral("/../../tts_config.json"),
    };

    QString ttsConfigPath;

    for (const QString &candidate : candidatePaths)
    {
        const bool exists = QFile::exists(candidate);
        qDebug() << "[TTS]   checking:" << candidate << "->" << (exists ? "FOUND" : "not found");

        if (exists)
        {
            ttsConfigPath = QFileInfo(candidate).absoluteFilePath();
            qDebug() << "[TTS]   resolved:" << ttsConfigPath;
            break;
        }
    }

    if (!ttsConfigPath.isEmpty())
    {
        m_ttsClient->LoadConfig(ttsConfigPath);
    }
    else
    {
        qDebug() << "[TTS]   WARNING: tts_config.json not found in any search path!";
    }

    connect(m_ttsClient, &TtsClient::SynthesisFinished,
            this, &PetController::OnTtsSynthesisFinished);

    // 初始化 TTS 音频播放器
    m_ttsAudioPlayer = new TtsAudioPlayer(this);

    connect(m_ttsAudioPlayer, &TtsAudioPlayer::PlaybackFinished,
            this, &PetController::OnAudioPlaybackFinished);

    // 初始化更新定时器
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(UPDATE_INTERVAL_MS);

    connect(m_updateTimer, &QTimer::timeout, this, &PetController::OnUpdate);
    m_updateTimer->start();

    return true;
}

void PetController::OnMousePress(const QPoint &localPos)
{
    m_pendingHitType = m_hitRegion.GetHitType(localPos);
    m_dragStartGlobalPos = QCursor::pos();
    m_windowStartPos = m_position;
    m_dragMoved = false;
    m_isDragging = false;
}

void PetController::OnMouseMove(const QPoint &globalPos)
{
    const QPoint delta = globalPos - m_dragStartGlobalPos;

    if (!m_isDragging)
    {
        if (delta.manhattanLength() <= DRAG_MOVE_THRESHOLD_PIXELS)
        {
            return;
        }

        m_isDragging = true;
        m_dragMoved = true;
        m_stateMachine.DragStart();
        ShowInteractionBubble(PET_STATE::DRAGGING);
        emit StateChanged(m_stateMachine.GetCurrentState());
    }

    QPoint newPosition = m_windowStartPos + delta;
    ClampPositionToScreen(newPosition);

    m_position = newPosition;
    emit PositionChanged(m_position);
}

void PetController::OnMouseRelease(const QPoint &localPos)
{
    (void)localPos;

    if (m_isDragging)
    {
        m_isDragging = false;
        m_stateMachine.DragEnd();
        emit StateChanged(m_stateMachine.GetCurrentState());
    }
    else if (!m_dragMoved)
    {
        if (m_pendingHitType == HIT_TYPE::HEAD)
        {
            if (m_stateMachine.ClickHead())
            {
                ShowInteractionBubble(PET_STATE::TOUCH_HEAD);
                emit StateChanged(m_stateMachine.GetCurrentState());
            }
        }
        else if (m_pendingHitType == HIT_TYPE::BODY)
        {
            if (m_stateMachine.ClickBody())
            {
                ShowInteractionBubble(PET_STATE::TOUCH_BODY);
                emit StateChanged(m_stateMachine.GetCurrentState());
            }
        }
    }

    m_dragMoved = false;
    m_pendingHitType = HIT_TYPE::NONE;
}

QString PetController::GetCurrentFramePath() const
{
    return m_stateMachine.GetCurrentFrame().GetImagePath();
}

QPoint PetController::GetPosition() const
{
    return m_position;
}

void PetController::SetPosition(const QPoint &position)
{
    m_position = position;
    ClampPositionToScreen(m_position);
    emit PositionChanged(m_position);
}

bool PetController::HasBubble() const
{
    return m_bubbleMessage.IsVisible();
}

QString PetController::GetBubbleText() const
{
    return m_bubbleMessage.GetText();
}

bool PetController::IsDragging() const
{
    return m_isDragging;
}

PET_STATE PetController::GetCurrentState() const
{
    return m_stateMachine.GetCurrentState();
}

void PetController::SetMood(PET_MOOD mood)
{
    m_stateMachine.SetMood(mood);
}

void PetController::SetScreenBounds(const QRect &bounds)
{
    m_screenBounds = bounds;
}

void PetController::SetHitRegions(const QRect &head, const QRect &body, const QRect &drag)
{
    m_hitRegion.SetHeadRegion(head);
    m_hitRegion.SetBodyRegion(body);
    m_hitRegion.SetDragRegion(drag);
}

void PetController::SetFrameSize(const QSize &size)
{
    if (size.isValid())
    {
        m_frameSize = size;
    }
}

QSize PetController::GetFrameSize() const
{
    return m_frameSize;
}

void PetController::OnUpdate()
{
    m_stateMachine.Update(UPDATE_INTERVAL_MS);
    m_bubbleMessage.Update(UPDATE_INTERVAL_MS);

    const PET_STATE currentState = m_stateMachine.GetCurrentState();
    const bool isInIdleGroup = (currentState == PET_STATE::IDLE)
                               || (currentState == PET_STATE::WALKING)
                               || (currentState == PET_STATE::SAYING);

    if (isInIdleGroup)
    {
        const int value = QRandomGenerator::global()->bounded(IDLE_TRIGGER_INTERVAL_MS);

        if (value < UPDATE_INTERVAL_MS)
        {
            m_stateMachine.IdleTrigger();
        }
    }

    if (currentState == PET_STATE::WALKING)
    {
        UpdateWalkPosition(UPDATE_INTERVAL_MS);
    }

    if (m_lastState != currentState)
    {
        m_lastState = currentState;
        emit StateChanged(currentState);

        if (currentState == PET_STATE::SAYING)
        {
            // SAYING 状态由 OnTtsSynthesisFinished 中调用 EnterSayState 进入，
            // 此时动画、气泡、音频同步开始
            const QString clipName = m_stateMachine.GetCurrentClipName();
            qDebug() << "[TTS] === Entering SAYING state ===";
            qDebug() << "[TTS]   clipName:" << clipName;
            emit SayStarted(clipName);

            // 显示气泡
            if (!m_currentSayText.isEmpty())
            {
                qDebug() << "[TTS]   bubble shown:" << m_currentSayText;
                emit SayTextReady(m_currentSayText);
            }

            // 播放已合成的音频
            if (!m_pendingAudioPath.isEmpty()
                && (m_ttsAudioPlayer != nullptr))
            {
                qDebug() << "[TTS]   playing pre-synthesized audio:" << m_pendingAudioPath;
                m_ttsAudioPlayer->Play(m_pendingAudioPath);
            }
        }
        else
        {
            // 离开 SAYING 状态时清理
            m_currentSayText.clear();
            m_pendingAudioPath.clear();
        }
    }

    // 处理 Say 动作：IdleTrigger 概率命中后，先合成音频再进入状态
    if (m_stateMachine.ConsumeSayPending())
    {
        const QString sayAction = m_stateMachine.SelectRandomSayAction();

        qDebug() << "[TTS] Say pending, action:" << sayAction;

        if (sayAction.isEmpty())
        {
            qDebug() << "[TTS]   no say actions available, falling back to Idle";
            m_stateMachine.IdleTrigger();
        }
        else
        {
            // 选取台词
            m_currentSayText = SayDialog::GetRandomText(sayAction);
            m_pendingAudioPath.clear();

            qDebug() << "[TTS]   selected text:" << m_currentSayText;

            if ((m_ttsClient != nullptr) && m_ttsClient->IsConfigured()
                && !m_currentSayText.isEmpty())
            {
                // 停止当前播放
                if ((m_ttsAudioPlayer != nullptr) && m_ttsAudioPlayer->IsPlaying())
                {
                    m_ttsAudioPlayer->Stop();
                }

                // 唯一文件名
                const QString uniqueName = QStringLiteral("say_%1.wav")
                                           .arg(m_synthesisCounter);

                m_pendingAudioPath = m_tempDir.filePath(uniqueName);
                m_pendingSayAction = sayAction;
                m_synthesisCounter += 1;

                qDebug() << "[TTS]   synthesizing audio first, output:" << m_pendingAudioPath;
                m_ttsClient->Synthesize(m_currentSayText, m_pendingAudioPath);
            }
        }
    }

    const QString framePath = GetCurrentFramePath();

    if (!framePath.isEmpty())
    {
        emit FrameChanged(framePath);
    }

    emit BubbleChanged(m_bubbleMessage.IsVisible(), m_bubbleMessage.GetText());
}

void PetController::UpdateWalkPosition(int deltaTimeMs)
{
    if (deltaTimeMs <= 0)
    {
        return;
    }

    const int movePixels = (m_walkSpeedPixelsPerSecond * deltaTimeMs) / 1000;
    QPoint newPosition = m_position;
    const bool isWalkingLeft = m_stateMachine.GetCurrentFrame().GetImagePath().contains(
                                   QStringLiteral("walk.left"));

    if (isWalkingLeft)
    {
        newPosition.setX(newPosition.x() - movePixels);
    }
    else
    {
        newPosition.setX(newPosition.x() + movePixels);
    }

    const bool hitLeft = newPosition.x() <= m_screenBounds.left();
    const bool hitRight = (newPosition.x() + m_frameSize.width()) >= m_screenBounds.right();

    if (hitLeft || hitRight)
    {
        // 到达边界后调头向反方向行走，而不是停住
        const QString newAction = isWalkingLeft
                                  ? QStringLiteral("walk_right")
                                  : QStringLiteral("walk_left");
        m_stateMachine.StartWalking(newAction);
        return;
    }

    m_position = newPosition;
    ClampPositionToScreen(m_position);
    emit PositionChanged(m_position);
}

void PetController::ShowInteractionBubble(PET_STATE state)
{
    QString text;

    switch (state)
    {
    case PET_STATE::TOUCH_HEAD:
        text = GetRandomTouchHeadText();
        break;

    case PET_STATE::TOUCH_BODY:
        text = GetRandomTouchBodyText();
        break;

    case PET_STATE::DRAGGING:
        text = QStringLiteral("干嘛……");
        break;

    default:
        return;
    }

    m_bubbleMessage.Show(text, BUBBLE_DURATION_MS);
    emit BubbleChanged(true, text);
}

QString PetController::GetRandomTouchHeadText()
{
    const QStringList texts =
    {
        QStringLiteral("好舒服~"),
        QStringLiteral("再摸摸头嘛"),
        QStringLiteral("嗯？"),
        QStringLiteral("嘿嘿")
    };

    const int index = QRandomGenerator::global()->bounded(texts.size());
    return texts.at(index);
}

QString PetController::GetRandomTouchBodyText()
{
    const QStringList texts =
    {
        QStringLiteral("痒……"),
        QStringLiteral("别闹啦"),
        QStringLiteral("好害羞"),
        QStringLiteral("哼哼")
    };

    const int index = QRandomGenerator::global()->bounded(texts.size());
    return texts.at(index);
}

void PetController::ClampPositionToScreen(QPoint &position) const
{
    if (!m_screenBounds.isValid())
    {
        return;
    }

    if (position.x() < m_screenBounds.left())
    {
        position.setX(m_screenBounds.left());
    }

    if (position.y() < m_screenBounds.top())
    {
        position.setY(m_screenBounds.top());
    }

    if (position.x() > (m_screenBounds.right() - m_frameSize.width()))
    {
        position.setX(m_screenBounds.right() - m_frameSize.width());
    }

    if (position.y() > (m_screenBounds.bottom() - m_frameSize.height()))
    {
        position.setY(m_screenBounds.bottom() - m_frameSize.height());
    }
}

void PetController::OnTtsSynthesisFinished(const QString &filePath)
{
    qDebug() << "[TTS] OnTtsSynthesisFinished, filePath:" << filePath;

    // 检查参数有效性
    if (filePath.isEmpty())
    {
        qDebug() << "[TTS]   FAILED - empty filePath (synthesis error or server error)";
        m_pendingAudioPath.clear();
        m_pendingSayAction.clear();
        return;
    }

    // 检查音频文件是否存在
    if (!QFileInfo::exists(filePath))
    {
        qDebug() << "[TTS]   FAILED - audio file not found";
        m_pendingAudioPath.clear();
        m_pendingSayAction.clear();
        return;
    }

    // 如果有待处理的 Say 动作，先进入 SAYING 状态再播放
    if (!m_pendingSayAction.isEmpty())
    {
        const QString actionName = m_pendingSayAction;
        m_pendingSayAction.clear();

        qDebug() << "[TTS]   entering SAYING state with action:" << actionName;

        if (m_stateMachine.EnterSayState(actionName))
        {
            // EnterSayState 成功后，OnUpdate 下一帧检测到 SAYING，
            // 会播放 m_pendingAudioPath 中的音频
            return;
        }

        // 进入 SAYING 失败（用户在此期间点击/拖拽了宠物），
        // 丢弃本次合成的音频，不做任何播放
        qDebug() << "[TTS]   EnterSayState failed - pet was interrupted, discarding audio";

        if (!m_pendingAudioPath.isEmpty())
        {
            QFile::remove(m_pendingAudioPath);
            m_pendingAudioPath.clear();
        }

        return;
    }

    // 直接播放（宠物已在 SAYING 状态，例如由其他路径触发）
    if (m_ttsAudioPlayer != nullptr)
    {
        qDebug() << "[TTS]   playing audio directly...";
        m_ttsAudioPlayer->Play(filePath);
    }
}

void PetController::OnAudioPlaybackFinished()
{
    qDebug() << "[TTS] OnAudioPlaybackFinished - audio playback done";

    // 告诉状态机退出 SAYING 的 B 循环，进入 C→IDLE
    if (m_stateMachine.GetCurrentState() == PET_STATE::SAYING)
    {
        qDebug() << "[TTS]   requesting exit from SAYING loop";
        m_stateMachine.RequestExitLoop();
    }

    // 播放完毕后删除音频文件以节约磁盘空间
    if (!m_pendingAudioPath.isEmpty())
    {
        QFile audioFile(m_pendingAudioPath);

        if (audioFile.exists())
        {
            const bool removed = audioFile.remove();
            qDebug() << "[TTS]   deleted audio file:" << m_pendingAudioPath
                     << "success:" << removed;
        }

        m_pendingAudioPath.clear();
    }

    // 清理 SAYING 相关状态
    m_currentSayText.clear();
}

} // namespace vpet
