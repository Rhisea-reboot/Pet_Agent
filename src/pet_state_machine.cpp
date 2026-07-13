#include "vpet/pet_state_machine.h"

#include <QRandomGenerator>

namespace vpet
{

namespace
{

constexpr int SAY_PROBABILITY_PERCENT = 20;        ///< Idle 时切换 Say 的概率
constexpr int WALK_PROBABILITY_PERCENT = 20;       ///< Idle 时切换 Walk 的概率
constexpr int RANDOM_MAX_PERCENT = 100;            ///< 随机数上限

} // anonymous namespace

PetStateMachine::PetStateMachine(const AnimationResourceManager &resourceManager)
    : m_resourceManager(resourceManager)
    , m_mood(PET_MOOD::NORMAL)
    , m_currentState(PET_STATE::IDLE)
    , m_currentClip()
    , m_currentSegmentType(ANIMATION_TYPE::SINGLE)
    , m_frameIndex(0)
    , m_elapsedMs(0)
    , m_shouldExitLoop(false)
{
}

void PetStateMachine::SetMood(PET_MOOD mood)
{
    m_mood = mood;
}

void PetStateMachine::Initialize()
{
    m_shouldExitLoop = false;
    EnterState(PET_STATE::IDLE, PetStateToActionName(PET_STATE::IDLE));
}

bool PetStateMachine::IdleTrigger()
{
    const bool isInIdleGroup = (m_currentState == PET_STATE::IDLE)
                               || (m_currentState == PET_STATE::WALKING)
                               || (m_currentState == PET_STATE::SAYING);

    if (!isInIdleGroup)
    {
        return false;
    }

    const int value = QRandomGenerator::global()->bounded(RANDOM_MAX_PERCENT);

    if (value < SAY_PROBABILITY_PERCENT)
    {
        const QList<QString> sayActions = m_resourceManager.GetSayActionNames();

        if (!sayActions.isEmpty())
        {
            const int index = QRandomGenerator::global()->bounded(sayActions.size());
            EnterState(PET_STATE::SAYING, sayActions[index]);
            return true;
        }
    }

    if (value < (SAY_PROBABILITY_PERCENT + WALK_PROBABILITY_PERCENT))
    {
        const bool walkLeft = QRandomGenerator::global()->bounded(2) == 0;
        const QString actionName = walkLeft
                                   ? QStringLiteral("walk_left")
                                   : QStringLiteral("walk_right");

        EnterState(PET_STATE::WALKING, actionName);
    }
    else
    {
        EnterState(PET_STATE::IDLE, PetStateToActionName(PET_STATE::IDLE));
    }

    return true;
}

bool PetStateMachine::ClickHead()
{
    if (!CanPreempt(PET_STATE::TOUCH_HEAD))
    {
        return false;
    }

    return EnterState(PET_STATE::TOUCH_HEAD, PetStateToActionName(PET_STATE::TOUCH_HEAD));
}

bool PetStateMachine::ClickBody()
{
    if (!CanPreempt(PET_STATE::TOUCH_BODY))
    {
        return false;
    }

    return EnterState(PET_STATE::TOUCH_BODY, PetStateToActionName(PET_STATE::TOUCH_BODY));
}

bool PetStateMachine::DragStart()
{
    if (m_currentState == PET_STATE::DRAGGING)
    {
        return true;
    }

    return EnterState(PET_STATE::DRAGGING, PetStateToActionName(PET_STATE::DRAGGING));
}

bool PetStateMachine::DragEnd()
{
    if (m_currentState != PET_STATE::DRAGGING)
    {
        return false;
    }

    if (m_currentClip.HasSegment(ANIMATION_TYPE::C_END))
    {
        // 鼠标松开后立即播放落地动画，不再等待当前 B 段循环帧结束
        m_currentSegmentType = ANIMATION_TYPE::C_END;
        m_frameIndex = 0;
        m_elapsedMs = 0;
    }
    else
    {
        m_shouldExitLoop = true;
    }

    return true;
}

bool PetStateMachine::RequestExitLoop()
{
    if (m_currentSegmentType != ANIMATION_TYPE::B_LOOP)
    {
        return false;
    }

    m_shouldExitLoop = true;
    return true;
}

bool PetStateMachine::StartWalking(const QString &actionName)
{
    if ((m_currentState != PET_STATE::WALKING) && (m_currentState != PET_STATE::IDLE))
    {
        return false;
    }

    return EnterState(PET_STATE::WALKING, actionName);
}

void PetStateMachine::Update(int deltaTimeMs)
{
    if (deltaTimeMs < 0)
    {
        return;
    }

    const AnimationSegment *segment = m_currentClip.GetSegment(m_currentSegmentType);

    if ((segment == nullptr) || segment->IsEmpty())
    {
        ReturnToIdle();
        return;
    }

    const AnimationFrame &frame = segment->GetFrame(m_frameIndex);

    if (!frame.IsValid())
    {
        ReturnToIdle();
        return;
    }

    m_elapsedMs += deltaTimeMs;

    if (m_elapsedMs < frame.GetDurationMs())
    {
        return;
    }

    m_elapsedMs -= frame.GetDurationMs();
    m_frameIndex += 1;

    if (m_frameIndex >= segment->GetFrameCount())
    {
        OnSegmentFinished();
    }
}

PET_STATE PetStateMachine::GetCurrentState() const
{
    return m_currentState;
}

const AnimationFrame &PetStateMachine::GetCurrentFrame() const
{
    static const AnimationFrame INVALID_FRAME;
    const AnimationSegment *segment = m_currentClip.GetSegment(m_currentSegmentType);

    if (segment == nullptr)
    {
        return INVALID_FRAME;
    }

    return segment->GetFrame(m_frameIndex);
}

bool PetStateMachine::IsInLoopSegment() const
{
    return (m_currentSegmentType == ANIMATION_TYPE::B_LOOP);
}

bool PetStateMachine::HasValidFrame() const
{
    return GetCurrentFrame().IsValid();
}

QString PetStateMachine::GetCurrentClipName() const
{
    return m_currentClip.GetName();
}

bool PetStateMachine::EnterState(PET_STATE newState, const QString &actionName)
{
    const AnimationClip newClip = m_resourceManager.GetClip(actionName, m_mood);

    m_currentState = newState;
    m_currentClip = newClip;
    m_shouldExitLoop = false;
    m_frameIndex = 0;
    m_elapsedMs = 0;

    if (m_currentClip.HasSegment(ANIMATION_TYPE::SINGLE))
    {
        m_currentSegmentType = ANIMATION_TYPE::SINGLE;
    }
    else if (m_currentClip.HasSegment(ANIMATION_TYPE::A_START))
    {
        m_currentSegmentType = ANIMATION_TYPE::A_START;
    }
    else if (m_currentClip.HasSegment(ANIMATION_TYPE::B_LOOP))
    {
        m_currentSegmentType = ANIMATION_TYPE::B_LOOP;
    }
    else
    {
        if (newState != PET_STATE::IDLE)
        {
            ReturnToIdle();
        }

        return false;
    }

    const bool isPlayOnceState = (newState == PET_STATE::TOUCH_HEAD)
                                 || (newState == PET_STATE::TOUCH_BODY)
                                 || (newState == PET_STATE::SAYING);

    if (isPlayOnceState && m_currentClip.HasSegment(ANIMATION_TYPE::C_END))
    {
        // 点击类交互与说话只播放一次完整 A->B->C 序列，B 段不循环
        m_shouldExitLoop = true;
    }

    return true;
}

void PetStateMachine::OnSegmentFinished()
{
    switch (m_currentSegmentType)
    {
    case ANIMATION_TYPE::A_START:
        if (m_currentClip.HasSegment(ANIMATION_TYPE::B_LOOP))
        {
            m_currentSegmentType = ANIMATION_TYPE::B_LOOP;
            m_frameIndex = 0;
            m_elapsedMs = 0;
        }
        else
        {
            ReturnToIdle();
        }
        break;

    case ANIMATION_TYPE::B_LOOP:
        if (m_shouldExitLoop && m_currentClip.HasSegment(ANIMATION_TYPE::C_END))
        {
            m_currentSegmentType = ANIMATION_TYPE::C_END;
            m_frameIndex = 0;
            m_elapsedMs = 0;
        }
        else
        {
            m_frameIndex = 0;
            m_elapsedMs = 0;
        }
        break;

    case ANIMATION_TYPE::C_END:
    case ANIMATION_TYPE::SINGLE:
    default:
        ReturnToIdle();
        break;
    }
}

void PetStateMachine::ReturnToIdle()
{
    EnterState(PET_STATE::IDLE, PetStateToActionName(PET_STATE::IDLE));
}

bool PetStateMachine::CanPreempt(PET_STATE newState) const
{
    return GetPetStatePriority(newState) > GetPetStatePriority(m_currentState);
}

} // namespace vpet
