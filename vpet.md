## 思路

### 动画逻辑及软件思路

软件有一个角色，作为你的桌宠，可以在电脑中陪伴你，同时可以通过鼠标事件与其产生交互

1. 动画逻辑：
   1. 待机时：
      动画：有概率->播放~/Animation/Walk并朝对应方向移动，有概率->播放~/Animation/Nomal
   2. 被点击时互动：
      动画：点击头->随机播放~/Animation/Touch_Head,点击身体->随机播放~/Animation/Touch_Body
      气泡回复：一些文字
   3. 鼠标拖动角色：
      动画：被提起->~/Animation/Raise
      气泡回复：干嘛……

## 系统介绍
### 动画系统

1. **动画格式设计**
   - 支持PNG序列帧动画
   - 实现ABC三段式动画系统 (Start-Loop-End)
   - 动画类型分类 (必需17种基础类型)

2. **动画加载优化**
   - 大图合成技术 (将多帧合并为单张图片)
   - 缓存机制
   - 内存管理和延迟加载

## 动作类型 (AnimatType)

### 1. 四种动作类型
```cpp
enum class AnimatType
{
    Single,    // 动画只有一个动作
    A_Start,   // 开始动作
    B_Loop,    // 循环动作
    C_End,     // 结束动作
};
```

### 2. 动作类型详细说明

| 动作类型    | 英文名称 | 中文含义 | 用途说明                         |
| ----------- | -------- | -------- | -------------------------------- |
| **Single**  | Single   | 单一动作 | 完整的独立动画，不需要分段       |
| **A_Start** | A_Start  | 开始动作 | 动画序列的开始部分               |
| **B_Loop**  | B_Loop   | 循环动作 | 动画序列的循环部分（可重复播放） |
| **C_End**   | C_End    | 结束动作 | 动画序列的结束部分               |

## 动画ABC系统介绍

### 1. 设计理念
动画ABC系统是一个 **三段式动画架构**，将复杂动画分解为：
- **A段（Start）**：入场/准备阶段
- **B段（Loop）**：主体/持续阶段  
- **C段（End）**：退场/结束阶段

### 2. 路径识别规则
```cpp
// 从文件路径中识别动作类型的优先级
if (path_name.Remove("a") || path_name.Remove("start"))      // A_Start
else if (path_name.Remove("b") || path_name.Remove("loop"))  // B_Loop
else if (path_name.Remove("c") || path_name.Remove("end"))   // C_End
else if (path_name.Remove("single"))                        // Single
else // 默认为Single
```

### 3. 实际应用示例

以摸头动画为例：
```
touch_head_a_100.png  // 摸头开始动画，100ms一帧
touch_head_b_150.png  // 摸头循环动画，150ms一帧  
touch_head_c_120.png  // 摸头结束动画，120ms一帧
```

播放流程：
1. **A段播放一次** → 手接近头部
2. **B段循环播放** → 持续摸头动作
3. **C段播放一次** → 手离开头部

### 4. 文件命名规范

支持的关键词识别：
- **A段**：`a` 或 `start`
- **B段**：`b` 或 `loop` 
- **C段**：`c` 或 `end`
- **单段**：`single`

这种设计使得动画更加自然流畅，特别适合需要持续交互的场景，用户可以控制B段的循环次数，而A、C段确保动画的完整性。

## 关键特点总结

### 1. **必须动画**
```
Raised_Dynamic, Raised_Static, Touch_Head, Touch_Body, Nomal, Walk
```
这些是桌宠系统运行的基础动画，必须提供。

### 2. **ABC三段式支持**
```
Raised_Static, Touch_Head, Touch_Body, Walk
```
这些动画支持完整的开始→循环→结束序列。

### 3. **动画优先级**
- **核心功能** > **交互反馈** > **状态切换** > **扩展功能**
- **必须动画** > **可选动画**

### 4. **设计理念**
- **模块化**：每种动画类型职责明确
- **可扩展**：通过Common类型支持自定义动画
- **用户体验**：丰富的交互反馈和状态表现
- **系统完整性**：涵盖了桌宠的完整生命周期

### 5. **状态机实现**
基于 WPF 事件驱动 + 文件系统约定 实现轻量级动画状态管理

状态机的核心思路可以概括为 **"优先级抢占 + ABC 自动衔接"**：

#### 1. 状态分层

只设 5 个状态，覆盖全部交互：

```
Idle        → 待机，可随机触发 Normal 或 Walk
Walking     → 移动
TouchHead   → 摸头交互
TouchBody   → 摸身体交互
Dragging    → 被鼠标拖动（最高优先级）
```

**优先级**：`Dragging(3) > TouchHead/TouchBody(2) > Walking(1) > Idle(0)`

#### 2. 两条核心规则

**规则 A：高优先级可抢占低优先级**
- 正在 `Walk_B` 循环时，用户点击头部 → 直接终止 Walk，进入 `TouchHead_A`
- 正在 `TouchHead_B` 循环时，用户按下鼠标拖动 → 直接终止，进入 `Raise_A`

**规则 B：同优先级或低优先级不能打断**
- 正在 `TouchHead_B` 时，定时器触发待机 → 忽略，继续交互
- 正在 `Dragging` 时，任何点击事件 → 忽略

#### 3. ABC 段流转逻辑

每个支持 ABC 的动画在状态机内部走同一条流水线：

```
事件触发 → 进入 A_Start → 播完自动进 B_Loop → [循环等待] → 进 C_End → 播完自动回 Idle
```

**B 段何时退出？**
- **正常流程**：收到外部退出信号（如松开鼠标）→ 当前循环帧播完后进 C 段
- **被打断流程**：高优先级事件到来 → **直接切走，不播 C 段**

```
被打断：A → B → [事件打断] → 直接切新状态 A
正常完：A → B → [收到退出信号] → C → Idle
```

#### 4. 事件入口设计

状态机只暴露 4 个外部事件，UI 层只调用这些：

| 事件 | 行为 |
|------|------|
| `IdleTrigger()` | 定时器触发，Idle 状态下按概率切 Walking 或重播 Normal |
| `ClickHead()` | 请求进入 TouchHead，检查优先级是否允许 |
| `ClickBody()` | 请求进入 TouchBody，检查优先级是否允许 |
| `DragStart()` / `DragEnd()` | DragStart 直接抢占；DragEnd 发信号让 B 段退出，走 C 段回 Idle |

#### 5. 状态机内部只需维护 3 个变量

```cpp
PetState currentState;      // 当前状态
AnimatClip* currentClip;    // 当前播放的段（A/B/C/Single）
bool shouldExitLoop;        // B 段是否收到退出信号
```

**每帧更新**：
1. 推进当前帧，时间到则切下一帧
2. 当前段播完 → 根据段类型决定下一步：
   - A → 找同动作的 B 段
   - B → 检查 `shouldExitLoop`，是则找 C 段，否则继续循环
   - C/Single → 回 Idle

#### 6. 与文件系统的映射

状态机不直接读文件，只通过 **动作名** 向资源管理器请求段：

```
状态        动作名              初始段
─────────────────────────────────────
Idle        "normal"            Single
Walking     "walk"              A_Start
TouchHead   "touch_head"        A_Start
TouchBody   "touch_body"        A_Start
Dragging    "raised_static"     A_Start
```

资源管理器负责把 `touch_head` + `A_Start` 解析成 `touch_head_a_100.png`。