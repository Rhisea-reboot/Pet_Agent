# VPet 项目当前开发报告

更新日期：2026-08-04

本文档按当前工作树和 `HEAD` 提交 `ae173a9` 更新。早期“尚未实现 Agent Runtime”“截图仍由 MainWindow 直接管理”“CTest 只有少量目标”等结论已经过时；历史过程请查看 [DEVELOPMENT_LOG.md](DEVELOPMENT_LOG.md)。

## 1. 项目概况

VPet 是基于 Qt 6 / C++17 的 Windows 桌面宠物应用，当前主路径包括：

- PNG 序列帧动画、状态机、拖拽和触摸交互。
- 气泡输出、语音输入和本地 GPT-SoVITS TTS。
- `PerceptionPipeline` 驱动的定时截图、处理链、环形帧缓冲和 Base64 编码。
- 配置驱动的 Agent DAG，支持用户输入和视觉感知两类触发。
- 文本 LLM、视觉 LLM、受预算限制的联网研究和情绪改写节点。
- CTest 单元/集成测试，以及本地 open-webSearch daemon 的受控集成测试。

产品仍未完成情绪到动画的闭环、分层记忆、流式 LLM、工具调用循环和更细粒度的主动打扰控制。这些是明确的后续能力，不应再被描述为当前已实现功能。

## 2. 构建与测试

构建系统使用 CMake，目标为 `VPet`，C++ 标准为 C++17，启用 Qt 的 `Core`、`Gui`、`Widgets`、`Network`、`Multimedia`，测试额外使用 `Qt6::Test`。

应用源文件已拆分为动画、感知、语音、LLM、web 和 agent 子目录。Agent Runtime 的实现分为：

- `agent_runtime.cpp`：生命周期、配置加载、公开入口和上下文访问。
- `agent_runtime_nodes.cpp`：默认节点处理器和输出策略。
- `agent_runtime_scheduler.cpp`：ready queue、pending 登记、invocation 排队和恢复。
- `agent_runtime_async.cpp`：文本、视觉和 web 异步回调处理。
- `agent_runtime_internal.h`：运行时内部常量和共享辅助定义。

当前 CTest 目标为 6 个：

1. `agent_dag_graph_tests`
2. `agent_runtime_scheduler_tests`
3. `application_integration_tests`
4. `web_search_client_tests`
5. `web_research_engine_tests`
6. `web_search_daemon_integration_tests`

Windows 下应运行：

```powershell
.\scripts\Run-Tests.ps1
```

脚本会根据构建目录的 `CMakeCache.txt` 为 CTest 子进程补齐 Qt 和匹配 MinGW 的运行时路径。直接运行 CTest 如果出现 `0xc0000135`，通常是 Qt DLL 未在 `PATH` 中，而不是测试逻辑失败。

## 3. 当前架构

```text
截图 -> PerceptionPipeline -> MainWindow -> AgentRuntime
语音/文本 -------------------------------> AgentRuntime

AgentRuntime
  -> AgentGraphExecutor
  -> AgentNodeRegistry
  -> AgentAsyncBridge
  -> InvocationQueuePolicy
  -> LlmClient / VisionLlmClient / WebResearchEngine
  -> AgentOutputReady -> MainWindow -> 气泡 / TTS / 动画
```

`AgentGraphExecutor` 负责触发源裁剪、拓扑调度、分支上下文和 Join；`AgentNodeRegistry` 按节点类型分发处理器；`AgentAsyncBridge` 按 `clientType:requestId` 关联异步回调。Runtime 同一时刻只执行一个 invocation，用户输入在活动轮次期间 FIFO 排队，视觉输入使用 latest-wins。

Invocation 以 session context 为基座，各分支保存本轮增量。Join 对冲突 key 默认报错，可通过 `config.merge` 使用 `prefer_user`、`prefer_vision` 或 `concat`。失败轮次不会提交本轮上下文，成功轮次目前只提交允许持久化的 `conversation.history`。

## 4. 已实现模块

### 4.1 桌宠 UI、动画和 TTS

`MainWindow` 管理窗口和用户事件，`PetController` 管理桌宠状态、位置、气泡、TTS 和音频播放。TTS 服务由 `TtsServerManager` 启动和健康检查，`TtsClient` 发送 HTTP 合成请求，`TtsAudioPlayer` 使用 Qt 多媒体播放音频。TTS 不可用时仍可显示文字气泡。

### 4.2 视觉感知

`PerceptionPipeline` 已替代 MainWindow 直接管理截图传感器的早期实现。它组合 `ScreenshotSensor`、`FrameBuffer`、`VisionEncoder` 和可选处理器链，启用后按 3 秒间隔产生 PNG Base64 数据。屏幕感知是右键菜单开启的隐私 opt-in，启动时默认关闭。MainWindow 将 `DataReady` 转发到 `AgentRuntime::UpdatePerceptionFrame`。

当前去重为编码内容 SHA-256；未实现感知级相似度检测。默认不保存截图到磁盘。

### 4.3 Agent DAG Runtime

当前默认 DAG 为：

```text
user.input -> web.research -> llm.chat -> emotion.rewrite -> output.format
vision.input -> vision.llm -> proactive.topic -> llm.chat
```

Runtime 启动时自动加载 `llm_config.json`、`vision_llm_config.json` 和 `web_search_config.json`。DAG 未找到或可选配置缺失时只记录 warning，不阻塞桌宠启动。

当前默认节点：

| 节点 | 当前职责 |
|---|---|
| `user.input` | 用户触发源 |
| `vision.input` | 校验和同步视觉帧 |
| `vision.llm` | 视觉摘要异步请求 |
| `proactive.topic` | 冷却、摘要去重和主动话题提示词 |
| `web.research` | 自动/显式联网研究、证据和引用上下文 |
| `llm.chat` | 文本 LLM 请求，支持 `temperature`、`top_p`、`frequency_penalty`、`presence_penalty`、`max_tokens` 校验 |
| `emotion.rewrite` | 情绪标签和回复改写 |
| `output.format` | 最终输出、来源和会话历史 |

节点间跨模块数据使用 `semantic.*` 和 `node.*` key，协议详见 [AGENT_CONTEXT_KEY_PROTOCOL.md](AGENT_CONTEXT_KEY_PROTOCOL.md)。

### 4.4 联网研究

`WebResearchEngine` 实现 `Decide -> Search -> Observe -> Assess -> Repeat / Compose` 状态机，默认最多 3 轮、每轮最多 2 个 query、最多 8 条结果、15 秒总预算和 6000 字符 Compose 上下文。研究结果只属于当前 invocation，外部标题、摘要和 URL 按不可信数据处理。

底层 `WebSearchClient` 只访问本地 daemon 的 `POST /search`。当前默认 daemon 为回环地址 `127.0.0.1:3210`，引擎为 Bing `request` 模式；daemon 不可用时默认 `failure_policy=continue` 降级为普通对话并保留限制说明。

## 5. 配置状态

| 文件 | 用途 | Git 状态 |
|---|---|---|
| `agent_dag_structure.json` | 默认 Agent DAG | 跟踪 |
| `agent_dag_structure.example.json` | DAG 示例 | 跟踪 |
| `llm_config.example.json` | 文本 LLM 模板 | 跟踪 |
| `vision_llm_config.example.json` | 视觉 LLM 模板 | 跟踪 |
| `web_search_config.example.json` | 搜索客户端模板 | 跟踪 |
| `llm_config.json` | 文本 LLM 实际配置 | Git 忽略 |
| `vision_llm_config.json` | 视觉 LLM 实际配置 | Git 忽略 |
| `web_search_config.json` | 搜索实际配置 | Git 忽略 |
| `tts_config.json` | 本地 TTS 配置 | 当前跟踪，部署前应检查个人路径和文本 |

配置查找通常覆盖可执行文件目录、当前工作目录和项目上级目录。真实 API key 不应提交到仓库。

## 6. 当前限制与风险

- `PetController` 仍承担较多 UI、状态和 TTS 协调职责。
- 文本和视觉 LLM 客户端尚无 SSE 流式输出、通用重试/退避、请求取消和并发上限。
- 主动发话只有固定冷却和摘要指纹去重，还没有用户忙碌、播放中和专注模式判断。
- 情绪标签尚未驱动动画状态；现有动画资产和自由标签之间缺少稳定映射。
- 会话历史是当前已持久化的主要 session 数据，尚无 working/semantic/episodic 分层记忆和磁盘 checkpoint。
- 当前没有通用 tool-calling 循环；联网研究是固定预算的专用节点内部状态机。
- 项目暂无 CI 配置，跨机器构建仍依赖本地 Qt/CMake 环境。

## 7. 验证状态

最近的 `ae173a9` 已包含 Agent Runtime 拆分、调度加固、测试脚本和回归测试。本次文档同步后已在隔离的 `build/doc-validation` 目录重新配置并构建，`scripts/Run-Tests.ps1 -BuildDirectory build/doc-validation` 运行 CTest 6/6 通过；`git diff --check` 未发现空白错误。

## 8. 后续建议

1. 为 CMake/Qt/CTest 建立 Windows CI，减少本地环境差异。
2. 将 TTS 配置模板化，并明确公开仓库中的许可证和资源分发边界。
3. 在现有异步桥上增加可取消请求和有限退避策略。
4. 为主动策略增加用户忙碌、播放中和专注模式输入。
5. 在不破坏无环 DAG 约束的前提下评估受限 tool-calling 节点。
6. 根据资产和产品决策补充情绪到动画的稳定映射，再实现动画联动。
