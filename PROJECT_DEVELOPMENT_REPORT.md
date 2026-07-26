# VPet 项目整体开发报告

## 1. 项目概述

VPet 当前是一个基于 Qt 6 / C++17 的桌面宠物应用。项目已经具备基础桌宠渲染、动画状态机、鼠标交互、TTS 语音、聊天气泡、自动截图感知和纯文本 LLM HTTP 客户端模块。

当前开发方向已经从单纯桌宠动画系统扩展为具备 Agent 能力的桌面伴随应用。根据 `vpet.md` 和 `FRAMEWORK.md`，后续目标是实现一个配置驱动的节点化 Agent 框架，支持被动用户输入和主动环境感知两种触发模式。

## 2. 当前代码结构

### 2.1 构建系统

项目使用 `CMakeLists.txt` 管理构建，目标为单个 Qt 可执行程序 `VPet`。

已启用模块：

- `Qt6::Core`
- `Qt6::Gui`
- `Qt6::Widgets`
- `Qt6::Network`
- `Qt6::Multimedia`

当前构建标准为 C++17，启用了 `CMAKE_AUTOMOC`，适合 QObject 派生类和 Qt signals/slots。

### 2.2 源码目录

主要目录如下：

- `include/vpet/`：公共头文件
- `src/`：模块实现文件
- `src/perception/`：视觉编码模块
- `src/sensor/`：自动截图传感器
- `src/llm/`：纯文本 LLM HTTP 客户端
- `GPT-SoVITS/`：TTS 服务相关外部组件
- `docs/`：已有设计或集成文档

## 3. 已实现模块

### 3.1 应用启动与窗口模块

相关文件：

- `src/main.cpp`
- `src/main_window.h`
- `src/main_window.cpp`

职责：

- 创建 `QApplication`
- 显示启动画面
- 启动并等待 TTS 服务状态
- 初始化 `MainWindow`
- 初始化动画资源路径
- 管理桌宠窗口、聊天气泡窗口和自动截图传感器

当前实现特点：

- 启动阶段有 TTS 服务等待逻辑和安全超时
- 主窗口透明、无边框、置顶、不抢焦点
- 主窗口只做 UI 展示和事件转发，不直接管理动画状态细节

### 3.2 动画资源模块

相关文件：

- `include/vpet/animation_frame.h`
- `include/vpet/animation_segment.h`
- `include/vpet/animation_clip.h`
- `include/vpet/animation_resource_manager.h`
- `src/animation_frame.cpp`
- `src/animation_segment.cpp`
- `src/animation_clip.cpp`
- `src/animation_resource_manager.cpp`

职责：

- 加载 PNG 序列帧资源
- 按动作名称、情绪、分段类型组织动画资源
- 支持 ABC 三段式动画结构
- 为状态机提供当前动作对应的动画剪辑

当前实现特点：

- 资源管理与状态机分离
- 状态机通过动作名请求动画，不直接扫描文件系统
- 支持情绪变体回退机制

### 3.3 状态机模块

相关文件：

- `include/vpet/pet_state_machine.h`
- `src/pet_state_machine.cpp`

职责：

- 管理桌宠顶层状态
- 实现 Idle、Walking、Saying、TouchHead、TouchBody、Dragging 状态
- 实现优先级抢占规则
- 实现 A_START、B_LOOP、C_END、SINGLE 动画段自动流转

当前实现特点：

- 状态数量较少，符合轻量状态机设计
- 高优先级状态可抢占低优先级状态
- `SAYING` 状态与 TTS 合成结果配合，避免动画先播、音频后到的错位问题

代码风险：

- `PetStateMachine::Update()` 只在单次调用中推进一帧。如果 `deltaTimeMs` 远大于当前帧时长，动画不会追帧，可能导致低帧率或卡顿后动画速度变慢。
- `GetCurrentFrame()` 返回静态无效帧对象，当前可用，但后续若引入多线程读取需要重新评估线程安全。

### 3.4 控制器模块

相关文件：

- `include/vpet/pet_controller.h`
- `src/pet_controller.cpp`

职责：

- 连接 UI 鼠标事件和状态机事件
- 管理桌宠位置、拖拽、边界限制和命中区域
- 管理交互气泡文本
- 管理 TTS 文本选择、音频合成、播放和状态机 SAYING 流程

当前实现特点：

- 控制器承担桌宠核心运行时协调职责
- TTS 与 SAYING 状态的顺序已经修正为先合成音频、再进入 SAYING 状态
- 交互事件和状态机分离较清晰

代码风险：

- `PetController` 当前职责较多，已经同时承担状态调度、位置控制、TTS、气泡和音频路径管理。后续接入 Agent 后不建议继续扩大该类职责。
- TTS 配置查找逻辑写在 `PetController::Initialize()` 内，后续可抽出统一配置查找工具，避免 LLM 配置查找重复实现。

### 3.5 TTS 模块

相关文件：

- `include/vpet/tts_client.h`
- `src/tts_client.cpp`
- `include/vpet/tts_audio_player.h`
- `src/tts_audio_player.cpp`
- `include/vpet/tts_server_manager.h`
- `src/tts_server_manager.cpp`

职责：

- 管理 GPT-SoVITS 服务进程
- 发送 TTS HTTP 请求
- 保存返回的 WAV 音频
- 播放合成音频

当前实现特点：

- TTS 服务启动有健康检查和超时保护
- TTS 请求异步执行
- 音频播放结束后通知状态机退出 SAYING 循环

代码风险：

- `TtsClient` 中仍有较多调试日志，适合当前调试阶段，但正式版本应降低日志量或加日志级别开关。
- TTS 配置包含本地服务路径和参考音频路径，部署时需要明确目录约定。

### 3.6 视觉感知模块

相关文件：

- `include/vpet/sensor/screenshot_sensor.h`
- `src/sensor/screenshot_sensor.cpp`
- `include/vpet/perception/vision_encoder.h`
- `src/perception/vision_encoder.cpp`

职责：

- 定时抓取屏幕图像
- 将截图编码为 PNG/JPG 字节流
- 将图像编码为 Base64，供后续发送到 LLM 或 Agent 节点
- 可选支持多屏拼接和保存到磁盘

当前实现特点：

- `ScreenshotSensor` 与 UI 动画系统低耦合
- `VisionEncoder` 独立负责图像编码
- 默认不保存截图到磁盘，降低隐私风险和 IO 开销
- 主窗口启动后默认每 3000ms 截图一次，并通过 `PerceptionReceived` 信号输出 Base64

代码风险：

- 自动截图当前直接在 `MainWindow` 中创建，后续应迁移到 Agent 模块或 PerceptionPipeline。
- 截图功能当前没有可视化调试开关。默认不打成功日志是合理的，但开发阶段需要额外工具确认定时输出。
- `FRAMEWORK.md` 中规划的 `FrameBuffer` 和 `PerceptionPipeline` 尚未落地。

### 3.7 纯文本 LLM 客户端模块

相关文件：

- `include/vpet/llm/llm_client.h`
- `src/llm/llm_client.cpp`
- `llm_config.example.json`

职责：

- 通过 OpenAI-compatible `/chat/completions` 接口发送纯文本消息
- 支持单轮 prompt 和多轮 messages
- 异步返回模型文本回复
- 统一处理网络错误、HTTP 错误和 JSON 解析错误

当前实现特点：

- `base_url`、`api_key`、`model` 均从配置文件读取
- 不支持多模态，不处理图像输入
- 不直接接入 UI，不直接接入截图模块
- 使用 `QNetworkAccessManager` 和 signals/slots 保持 Qt 异步模型
- 使用 `qScopeGuard` 管理 `QNetworkReply::deleteLater()` 调用

代码风险：

- 当前只有 `llm_config.example.json`，还没有指定正式配置文件名和自动加载路径。
- API Key 放在配置文件中会带来泄漏风险，仓库应避免提交真实 `llm_config.json`。
- 当前只支持非流式请求，暂不支持 SSE 流式输出。
- 当前没有请求取消、并发请求上限、速率限制退避机制。

## 4. Agent 方向进度

根据 `vpet.md`，后续 Agent 架构目标是配置驱动的 DAG 节点编排引擎。当前代码已经完成了两个关键底座模块：

- 主动输入底座：`ScreenshotSensor` + `VisionEncoder`
- 文本推理底座：`LlmClient`

尚未完成的 Agent 核心：

- Agent 模块注册中心
- 节点接口定义
- DAG 配置解析
- 拓扑排序执行器
- 上下文记忆模块
- 情感分析节点
- 用户画像固化模块
- LLM 节点封装
- 截图节点封装
- 错误降级和条件路由机制

## 5. 配置文件状态

当前已有配置或模板：

- `tts_config.json`：TTS 实际配置文件
- `llm_config.example.json`：LLM 配置模板

当前缺失：

- `llm_config.json`：LLM 实际配置文件
- LLM 配置自动查找逻辑
- Agent DAG 配置文件
- 用户画像存储配置

建议下一步确定：

- LLM 默认配置文件名使用 `llm_config.json`
- 配置查找顺序与 TTS 保持一致
- 将真实 `llm_config.json` 加入 `.gitignore`

## 6. 代码规范审查摘要

### 6.1 总体评价

当前新增模块整体遵守了项目 C++ 代码规范：

- 模块已拆分为独立头文件和源文件
- 自定义结构体使用 `_tag` 前缀
- 枚举类型使用全大写和下划线
- 关键函数声明有注释说明功能、参数和返回值
- 动态资源大多通过 QObject 父子关系托管
- 新增 LLM 模块没有硬编码真实敏感数据
- 新增截图模块默认不落盘，降低隐私风险

### 6.2 主要待改进点

#### 1. `PetController` 职责过重

严重程度：中

影响：后续继续接入 Agent 后，`PetController` 可能变成上帝类。

建议：将 TTS 流程、Agent 流程、配置查找流程拆为独立模块或服务类。

#### 2. LLM 配置尚未自动加载

严重程度：中

影响：模块已经实现，但程序启动后不会自动读取 `llm_config.json`。

建议：实现 `LlmConfigLocator` 或通用配置查找函数，并在 Agent 初始化阶段加载。

#### 3. 自动截图当前接入位置临时

严重程度：中

影响：截图已能运行，但从架构上应属于 Agent/PerceptionPipeline，不应长期由 `MainWindow` 管理。

建议：实现 `PerceptionPipeline` 后，将 `ScreenshotSensor` 从 `MainWindow` 迁移出去。

#### 4. 缺少自动化测试

严重程度：中

影响：构建已验证，但缺少单元测试覆盖 JSON 解析、边界参数、HTTP 响应解析。

建议：引入 Qt Test 或轻量测试目标，优先测试 `LlmClient` JSON 解析和 `VisionEncoder` 编码。

#### 5. TTS 调试日志偏多

严重程度：低

影响：正式运行时输出噪声较大，可能影响问题定位。

建议：引入日志级别或编译期开关。

## 7. 验证情况

最近已完成的验证：

- 使用 Qt 6.9.2 MinGW 64-bit 构建通过
- `VPet.exe` 成功链接
- `git diff --check` 无空白错误
- LLM 模块在无真实 API Key 的情况下完成编译级验证
- 截图模块完成编译级验证，并已接入主窗口启动流程

构建命令：

```powershell
$env:PATH = "E:\Qt\Tools\mingw1310_64\bin;E:\Qt\Tools\Ninja;E:\Qt\Tools\CMake_64\bin;E:\Qt\6.9.2\mingw_64\bin;$env:PATH"
& "E:\Qt\Tools\CMake_64\bin\cmake.exe" --build "F:\Pet Agent\build\opencode-screenshot"
```

未完成验证：

- 未使用真实 LLM API Key 做端到端请求验证
- 未用真实 `llm_config.json` 验证启动加载
- 未做长时间截图稳定性测试
- 未做 TTS 服务缺失时的完整降级体验测试

## 8. Git 状态

近期关键提交：

- `7313fd4 Add automatic screenshot sensor`
- `599ecab Add text LLM client`
- `e88a969 Read LLM credentials from config`

当前工作区存在未提交文件或改动：

- `CMakeLists.txt.user`
- `vpet.md`
- `FRAMEWORK.md`
- `情感.md`
- `通用的llmapi的调用方法.md`

这些文件目前不是本报告生成前的已提交功能变更，其中 `CMakeLists.txt.user` 属于 Qt Creator 用户配置，不建议纳入版本控制。

## 9. 后续开发建议

### 9.1 近期优先级

1. 确定并实现 `llm_config.json` 自动查找。
2. 将真实 `llm_config.json` 加入 `.gitignore`。
3. 建立 `Agent` 或 `AgentRuntime` 基类，统一管理 LLM、截图、TTS 等模块生命周期。
4. 实现 LLM 节点封装，让 `LlmClient` 成为节点执行器的底层客户端。
5. 实现最小上下文管理，支持用户输入和模型回复的多轮消息缓存。

### 9.2 中期目标

1. 实现 DAG 配置解析和拓扑执行。
2. 实现被动文本输入链路：用户输入到 LLM 到气泡和 TTS 输出。
3. 实现主动截图链路：截图到视觉分析节点到反馈策略。
4. 实现情感分析和用户画像固化模块。
5. 实现统一错误降级策略。

### 9.3 工程质量目标

1. 引入测试目标，覆盖 LLM 响应解析、配置校验和图像编码。
2. 拆分 `PetController` 中的 TTS 流程。
3. 抽象通用配置查找工具。
4. 给自动截图增加开发期观测方式，例如可配置保存最近一帧。
5. 建立发布模式日志策略。

## 10. 结论

项目当前已经从基础桌宠进入 Agent 能力搭建阶段。桌宠动画、交互、TTS、截图感知和文本 LLM 调用底座已经分别落地，模块边界总体清晰。

当前最大缺口不是单点能力，而是 Agent Runtime：需要一个统一的模块注册、配置加载、节点编排和上下文管理层，把已经实现的 `ScreenshotSensor`、`VisionEncoder`、`LlmClient`、TTS 和 UI 输出串成完整闭环。

建议下一阶段先完成 `llm_config.json` 自动加载和最小文本对话链路，再推进 DAG 节点编排。这样可以最快验证桌宠从“动画交互”升级到“可对话 Agent”的核心体验。
