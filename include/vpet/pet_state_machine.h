#ifndef VPET_PET_STATE_MACHINE_H
#define VPET_PET_STATE_MACHINE_H

#include "vpet/animation_clip.h"
#include "vpet/animation_frame.h"
#include "vpet/animation_resource_manager.h"
#include "vpet/common_types.h"

#include <QString>

namespace vpet
{

/**
 * @brief 桌宠状态机
 *
 * 实现 "优先级抢占 + ABC 自动衔接" 的状态管理：
 * - 高优先级状态可抢占低优先级状态
 * - 同优先级不可打断
 * - A 段播完自动进入 B 段，B 段收到退出信号后进入 C 段，C/Single 段播完回到 Idle
 */
class PetStateMachine
{
public:
    /**
     * @brief 构造函数
     * @param[in] resourceManager 动画资源管理器引用
     */
    explicit PetStateMachine(const AnimationResourceManager &resourceManager);

    /**
     * @brief 设置当前情绪
     *
     * 情绪决定优先使用哪套动画变体；资源缺失时自动回退。
     *
     * @param[in] mood 情绪
     */
    void SetMood(PET_MOOD mood);

    /**
     * @brief 初始化状态机，进入 Idle 状态
     */
    void Initialize();

    /**
     * @brief Idle 定时触发
     *
     * 在 Idle、Walking、Saying 任一待机状态下生效，按概率在三种待机动画间切换。
     *
     * @return 事件被处理返回 true
     */
    bool IdleTrigger();

    /**
     * @brief 请求进入摸头状态
     * @return 优先级允许并进入状态返回 true
     */
    bool ClickHead();

    /**
     * @brief 请求进入摸身体状态
     * @return 优先级允许并进入状态返回 true
     */
    bool ClickBody();

    /**
     * @brief 开始拖拽
     *
     * 拖拽为最高优先级，可直接抢占任何状态。
     *
     * @return 事件被处理返回 true
     */
    bool DragStart();

    /**
     * @brief 结束拖拽
     *
     * 向当前 B 段发送退出信号，播完当前循环后进入 C 段并回到 Idle。
     *
     * @return 当前处于拖拽状态并收到信号返回 true
     */
    bool DragEnd();

    /**
     * @brief 请求退出当前 B 段循环
     *
     * 当 B 段播完当前循环后进入 C 段（若存在），随后回到 Idle。
     *
     * @return 当前处于 B 段并收到信号返回 true
     */
    bool RequestExitLoop();

    /**
     * @brief 以指定方向进入行走状态
     *
     * 用于边界回弹时切换方向；仅在 Walking 状态或 Idle 状态下可用。
     *
     * @param[in] actionName 方向动作名，如 "walk_left", "walk_right"
     * @return 切换成功返回 true
     */
    bool StartWalking(const QString &actionName);

    /**
     * @brief 推进动画时间
     * @param[in] deltaTimeMs 距上一帧的时间间隔，单位毫秒，必须 >= 0
     */
    void Update(int deltaTimeMs);

    /**
     * @brief 获取当前状态
     * @return 当前状态
     */
    PET_STATE GetCurrentState() const;

    /**
     * @brief 获取当前帧
     * @return 当前帧的常量引用；无有效帧时返回无效帧
     */
    const AnimationFrame &GetCurrentFrame() const;

    /**
     * @brief 判断是否处于循环段
     * @return 当前段为 B_LOOP 返回 true
     */
    bool IsInLoopSegment() const;

    /**
     * @brief 判断当前是否有有效帧
     * @return 有有效帧返回 true
     */
    bool HasValidFrame() const;

    /**
     * @brief 获取当前动画剪辑名称
     * @return 当前剪辑名称；无有效剪辑返回空字符串
     */
    QString GetCurrentClipName() const;

private:
    /**
     * @brief 进入指定状态并加载对应动画
     * @param[in] newState 目标状态
     * @param[in] actionName 动作名
     * @return 成功加载到有效动画返回 true
     */
    bool EnterState(PET_STATE newState, const QString &actionName);

    /**
     * @brief 当前段播放结束后的流转处理
     */
    void OnSegmentFinished();

    /**
     * @brief 回到 Idle 状态
     */
    void ReturnToIdle();

    /**
     * @brief 判断新状态是否可以抢占当前状态
     * @param[in] newState 新状态
     * @return 新状态优先级高于当前状态时返回 true
     */
    bool CanPreempt(PET_STATE newState) const;

private:
    const AnimationResourceManager &m_resourceManager; ///< 动画资源管理器
    PET_MOOD m_mood;                                   ///< 当前情绪
    PET_STATE m_currentState;                          ///< 当前状态
    AnimationClip m_currentClip;                       ///< 当前动画剪辑
    ANIMATION_TYPE m_currentSegmentType;               ///< 当前播放段类型
    int m_frameIndex;                                  ///< 当前帧索引
    int m_elapsedMs;                                   ///< 当前帧已播放时间
    bool m_shouldExitLoop;                             ///< B 段是否收到退出信号
};

} // namespace vpet

#endif // VPET_PET_STATE_MACHINE_H
