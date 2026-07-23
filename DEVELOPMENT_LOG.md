# VPet 开发日志

## 2026-07-19

### 1. Agent 框架最小启动闭环

本次开发将原本仅具备 DAG 结构解析能力的 Agent 框架接入应用启动流程，使其具备最小可启动能力。

新增文件：

- `include/vpet/agent/agent_runtime.h`
- `src/agent/agent_runtime.cpp`
- `agent_dag_structure.json`

修改文件：

- `src/main.cpp`
- `CMakeLists.txt`

实现内容：

- 新增 `AgentRuntime` 运行时启动器。
- 支持从 `agent_dag_structure.json` 加载 Agent DAG 配置。
- 调用现有 `AgentDagGraph` 完成结构校验和拓扑排序。
- 在启动日志中输出 DAG 加载状态和拓扑执行顺序。
- 提供 `Load()`、`Execute()`、`Start()` 三个运行方法。
- 当前模块尚未完整实现，因此节点执行采用占位逻辑。
- 每个节点执行时输出日志，例如：`[Agent] Execute placeholder node: "parse_input"`。
- `main.cpp` 启动时查找 `agent_dag_structure.json`，找到后启动 Agent Runtime。
- Agent 启动失败或配置缺失时只输出 warning，不阻塞桌宠主程序启动。

当前效果：

- Agent 框架已经从“静态 DAG 解析器”变成“可启动的运行时骨架”。
- 后续可以逐步替换占位节点，实现真实模块逻辑。

后续建议：

- 为节点定义统一输入输出上下文。
- 将 `parse_input`、`image_recognition`、`call_llm` 等节点拆成独立执行器。
- 支持节点失败策略，例如跳过、重试、终止。
- 将 Agent 执行从启动期迁移到事件触发期，例如截图识别完成后触发。

### 2. 视觉 LLM 模型设置入口

本次开发为桌宠添加了右键菜单入口，用于切换图像识别模型。

修改文件：

- `src/main_window.cpp`
- `src/main_window.h`
- `include/vpet/llm/vision_llm_client.h`
- `src/llm/vision_llm_client.cpp`

实现内容：

- 右键桌宠时弹出菜单。
- 菜单中新增 `图像识别模型设置` 子菜单。
- 子菜单包含 `mimo-v2.5` 和 `gpt` 两个选项。
- 当前激活模型以 check 状态显示。
- `VisionLlmClient` 增加模型 profile 概念。
- `gpt` profile 使用 `max_tokens` 请求字段，并解析 `choices[0].message.content`。
- `mimo-v2.5` profile 使用 `max_completion_tokens` 请求字段，并解析 `choices[0].message.reasoning_content`。
- MiMo 请求中补充 system message，贴近 MiMo 官方示例。

当前限制：

- 菜单选项已经存在，但配置仍然不是完整的多模型配置文件。
- `mimo-v2.5` 可使用当前 `vision_llm_config.json`。
- `gpt` 预设依赖环境变量 `OPENAI_API_KEY`、`OPENAI_BASE_URL`、`OPENAI_MODEL`。
- 后续应将视觉模型配置改造成列表式配置，并支持持久化当前选择。

### 3. 视觉 LLM 响应调试日志

为排查 MiMo 返回 `content` 为空的问题，增加完整响应体日志。

修改文件：

- `src/llm/vision_llm_client.cpp`

实现内容：

- 在 `OnReplyFinished()` 中输出完整响应 JSON。
- 日志包含 request id、HTTP status 和完整 response body。
- 用于确认不同模型的真实响应结构。

排查结论：

- MiMo 返回 HTTP 200，说明请求已成功。
- MiMo 的回复文本位于 `choices[0].message.reasoning_content`。
- 原代码只解析 `choices[0].message.content`，因此报 `Vision LLM response content is empty.`。

### 4. Saying / TTS 稳定性修复

本轮修复了 saying 动画触发后偶发无声音、无气泡的问题。

修改文件：

- `src/pet_controller.cpp`
- `include/vpet/pet_controller.h`
- `src/pet_state_machine.cpp`
- `include/vpet/pet_state_machine.h`
- `src/tts_audio_player.cpp`
- `include/vpet/tts_audio_player.h`

主要问题：

- `SAYING` 状态期间仍可能随机触发新的 Say，导致 TTS 合成请求互相覆盖。
- `SAYING -> SAYING` 重入不会产生状态变化，控制器无法触发播放和气泡显示。
- TTS 合成失败时清空了台词，导致无音频时也没有气泡兜底。
- `QSoundEffect` 播放动态生成的 TTS WAV 时容易在加载或停止阶段误触发播放完成。

修复内容：

- 禁止 `SAYING` 状态下继续随机触发 Say。
- 禁止 `EnterSayState()` 执行 `SAYING -> SAYING` 重入。
- TTS 合成中已有待处理 Say 时，跳过新的 Say 请求。
- TTS 失败或未配置时，仍进入 `SAYING` 显示气泡，只是不播放音频。
- `RequestExitLoop()` 支持在 A 段提前排队退出，避免退出请求过早失效。
- TTS 播放器由 `QSoundEffect` 切换为 `QMediaPlayer + QAudioOutput`。
- 进入 `SAYING` 状态时同步更新 `BubbleMessage`，保证原有气泡通道也能显示台词。

当前效果：

- Saying 气泡显示更稳定。
- TTS 播放完成判断更可靠。
- 音频失败时仍能看到台词反馈。

### 5. 验证情况

已执行：

- `git diff --check`

结果：

- 未发现新增空白格式错误。
- 仅存在仓库已有的 LF/CRLF 换行提示。

未完成：

- 当前环境中 `cmake` 不可用，无法完成编译验证。
- `cmake --version` 报错：PowerShell 无法识别 `cmake` 命令。

### 6. 语音输入链路下沉与全局热键

本次开发将按键语音输入改为系统全局热键，并把临时的文本 LLM 直连逻辑下沉到 `AgentRuntime`。

修改文件：

- `src/main_window.cpp`
- `src/main_window.h`
- `src/agent/agent_runtime.cpp`
- `include/vpet/agent/agent_runtime.h`
- `src/main.cpp`
- `include/vpet/speech/voice_input_manager.h`
- `src/speech/voice_input_manager.cpp`

实现内容：

- 使用 Windows `RegisterHotKey()` 注册全局热键 `Ctrl+Alt+V`。
- 通过 `nativeEvent()` 接收 `WM_HOTKEY`，桌宠窗口不再需要焦点。
- 语音输入保持“按一次开始、再按一次停止并提交”的切换方式。
- `VoiceInputManager` 负责录音并调用 GPT-SoVITS 自带 `funasr_asr.py`。
- `AgentRuntime` 新增统一上下文对象 `AgentContext`，用于保存用户输入和节点执行痕迹。
- `AgentRuntime` 接管 `llm_config.json` 自动加载、`LlmClient` 持有与请求发送。
- `MainWindow` 只负责把 ASR 文本交给 `AgentRuntime`，并监听 Agent 日志和 LLM 结果信号。

当前效果：

- 语音输入不再需要窗口获得焦点。
- 文本推理职责从 `MainWindow` 下沉到 `AgentRuntime`，窗口层更轻。
- 语音文本已经能够进入统一上下文，并触发 LLM 请求。

当前限制：

- 语音触发仍是 `Ctrl+Alt+V`，不是单独 `V`，避免抢占正常输入。
- `AgentRuntime` 的 DAG 节点执行仍是占位逻辑，后续还需要把 `input / llm / log` 等节点真正编排起来。
- `VoiceInputManager` 目前优先走中文 ASR 脚本，未接入完整多语种自动选择策略。

验证情况：

- 已执行 `cmake --build "F:\Pet Agent\build\opencode-screenshot"`。
- 构建通过，已成功链接 `VPet.exe`。
- `git diff --check` 仅提示仓库已有的 LF/CRLF 换行差异，没有新增空白错误。

### 8. Agent DAG 节点对象化

本次开发完成了 Agent DAG 结构的第一步升级，让节点从纯字符串扩展为对象配置。

修改文件：

- `include/vpet/agent/agent_dag_graph.h`
- `src/agent/agent_dag_graph.cpp`
- `include/vpet/agent/agent_runtime.h`
- `src/agent/agent_runtime.cpp`
- `agent_dag_structure.json`
- `agent_dag_structure.example.json`

实现内容：

- 新增 `_tagAgentDagNode` 节点定义，包含 `id`、`type`、`config` 三个字段。
- `AgentDagGraph` 支持解析 `{ id, type, config }` 节点对象。
- 保留旧字符串节点格式兼容，字符串节点会自动映射为相同的 `id` 和 `type`。
- 拓扑排序仍输出节点 `id`，边定义继续使用节点 `id` 引用。
- 新增 `GetNode()`，支持运行时按节点 `id` 读取完整节点定义。
- `AgentRuntime::ExecuteNode()` 改为接收 `_tagAgentDagNode`，为后续按 `type` 分发执行打底。
- 当前执行仍保持占位逻辑，只额外记录节点 `type` 到上下文 `runtime.last_node_type`。
- `agent_dag_structure.json` 和示例配置已改为对象节点格式。

当前效果：

- DAG 配置已经具备节点类型和节点配置承载能力。
- 后续可以在不再修改 DAG 解析器的前提下，实现 `input.parse`、`llm.chat`、`output.format` 等真实节点执行器。
- 旧版字符串节点配置仍可加载，降低已有配置迁移风险。

验证情况：

- 已执行 `git diff --check`。
- 结果仅提示仓库已有 LF/CRLF 换行差异，没有新增空白错误。
- 已执行 `cmake --build "F:\Pet Agent\build\opencode-screenshot"`。
- 构建通过，已成功链接 `VPet.exe`。

### 9. AgentRuntime 最小真实节点链路

本次开发将 Agent Runtime 从统一占位执行推进到按节点类型分发，并落地最小文本链路。

修改文件：

- `include/vpet/agent/agent_runtime.h`
- `src/agent/agent_runtime.cpp`
- `src/main.cpp`
- `agent_dag_structure.json`
- `agent_dag_structure.example.json`

实现内容：

- `AgentRuntime::ExecuteNode()` 改为按节点 `type` 分发。
- 新增 `input.parse` 节点，负责检查用户输入是否存在，并写入 `input.available`。
- 新增 `prompt.assemble` 节点，负责从用户输入组装 `prompt.text`。
- 新增 `llm.chat` 节点，负责通过 `LlmClient` 发送文本 LLM 请求，并记录 `llm.last_request_id` 和 `llm.pending`。
- 新增 `output.format` 节点，负责在已有 LLM 回复时写入 `output.text`；异步回复未返回时记录等待状态。
- 未实现的节点类型改为显式 pass-through，写入 `runtime.pass_through.<node_id>`，避免继续使用模糊的 placeholder 语义。
- `ExecuteWithUserInput()` 在 DAG 已加载时只走节点链路，不再额外触发直接 LLM fallback，避免重复请求。
- 直接 LLM fallback 仅保留在 DAG 未加载时使用。
- 收敛当前 `agent_dag_structure.json` 为最小链路：`parse_input -> assemble_prompt -> call_llm -> format_output`。
- 更新 `main.cpp` 启动注释，说明语音输入会进入节点化链路。

当前效果：

- 语音输入进入 Agent 后可以通过 DAG 中的 `llm.chat` 节点发送 LLM 请求。
- 启动期执行 DAG 时如果没有用户输入，LLM 节点会安全跳过，不会发送空请求。
- LLM 回复完成后仍通过原有 `LlmResponseReceived` 信号返回给 UI，同时写入上下文 `llm.last_response` 和 `output.text`。

验证情况：

- 已执行 `git diff --check`。
- 结果仅提示仓库已有 LF/CRLF 换行差异，没有新增空白错误。
- 已执行 `cmake --build "F:\Pet Agent\build\opencode-screenshot"`。
- 构建通过，已成功链接 `VPet.exe`。

### 10. FrameBuffer 环形帧缓冲

本次开发实现了视觉感知框架中的基础帧缓冲模块，为后续 `PerceptionPipeline` 和视觉时序回溯打底。

新增文件：

- `include/vpet/perception/frame_buffer.h`
- `src/perception/frame_buffer.cpp`

修改文件：

- `CMakeLists.txt`

实现内容：

- 新增 `_tagFrame` 帧结构，包含 `pixmap`、`timestamp`、`sequenceId`、`filePath`。
- 新增 `FrameBuffer` 环形缓冲类，内部按最旧到最新存储，外部读取约定为 `0` 表示最新帧。
- 支持 `Push()` 写入帧，空图像帧不会写入。
- 支持容量归一化，容量范围限制为 `1` 到 `4096`。
- 支持 `GetLatest()`、`GetAt()`、`GetRecent()`、`GetSize()`、`GetCapacity()`、`IsEmpty()`、`Clear()`。
- 支持 `StitchRecent()` 按横向或纵向拼接最近 N 帧。
- 写入帧缺少有效时间戳时，会自动补充 UTC 时间戳。
- 索引越界、空缓冲、无效方向等情况会返回空图像或无效帧，避免数组越界和空对象参与绘制。
- 已接入 `CMakeLists.txt` 的源文件和头文件列表。

当前效果：

- 项目已经具备独立的最近帧缓存能力。
- 后续 `PerceptionPipeline` 可以直接组合 `ScreenshotSensor`、`FrameBuffer` 和 `VisionEncoder`。
- `FrameBuffer` 当前不依赖 UI，也不依赖 Agent Runtime。

验证情况：

- 已执行 `git diff --check -- CMakeLists.txt include/vpet/perception/frame_buffer.h src/perception/frame_buffer.cpp`。
- 结果仅提示仓库已有 LF/CRLF 换行差异，没有新增空白错误。
- 已执行 `cmake --build "F:\Pet Agent\build\opencode-screenshot"`。
- 构建通过，已成功链接 `VPet.exe`。

### 11. PerceptionPipeline 感知管道

本次开发实现了视觉感知框架中的基础感知管道，组合已有截图传感器、帧缓冲和视觉编码器。

新增文件：

- `include/vpet/perception/perception_pipeline.h`
- `src/perception/perception_pipeline.cpp`

修改文件：

- `CMakeLists.txt`

实现内容：

- 新增 `PerceptionPipeline::_tagConfig`，包含截图传感器配置、缓冲容量、编码格式、编码选项和缓冲开关。
- `PerceptionPipeline` 内部持有 `ScreenshotSensor`，通过 QObject 父子关系管理生命周期。
- `PerceptionPipeline` 内部组合 `FrameBuffer`，可选缓存处理后的最近帧。
- 支持 `Start()`、`Stop()`、`IsRunning()`、`CaptureOnce()`。
- 支持 `GetLatestEncodedData()` 获取最新编码结果。
- 支持 `GetRecentEncodedData()` 从缓冲中读取最近 N 帧并重新编码。
- 支持 `AddProcessor()` 和 `ClearProcessors()` 管理图像处理链。
- 截图完成后执行流程为：读取最新截图、应用处理链、编码图像、写入缓冲、发出 `DataReady`。
- 启用缓冲时会发出 `BatchReady`，输出最近帧批量编码结果。
- 对空 Base64、无效帧序号、无效尺寸、空截图、空处理结果、编码失败等情况均发出 `ErrorOccurred`。
- 管道会强制关闭传感器 `autoStart`，避免构造期间提前截图导致连接尚未建立。

当前效果：

- 项目已经具备独立后台视觉感知管道。
- 该管道当前尚未接入 `MainWindow` 或 `AgentRuntime`，避免一次性改动 UI 运行链路。
- 后续可以用它替换 `MainWindow` 中直接创建 `ScreenshotSensor` 的临时实现。

验证情况：

- 已执行 `git diff --check -- CMakeLists.txt include/vpet/perception/perception_pipeline.h src/perception/perception_pipeline.cpp`。
- 结果仅提示仓库已有 LF/CRLF 换行差异，没有新增空白错误。
- 已执行 `cmake --build "F:\Pet Agent\build\opencode-screenshot"`。
- 构建通过，已成功链接 `VPet.exe`。

### 12. MainWindow 迁移到 PerceptionPipeline

本次开发将 `MainWindow` 中直接创建和启动 `ScreenshotSensor` 的临时逻辑迁移到 `PerceptionPipeline`。

修改文件：

- `src/main_window.h`
- `src/main_window.cpp`
- `include/vpet/perception/perception_pipeline.h`
- `src/perception/perception_pipeline.cpp`

实现内容：

- `MainWindow` 不再前向声明、持有或删除 `ScreenshotSensor`。
- `MainWindow` 新增 `PerceptionPipeline *m_perceptionPipeline` 成员，由 QObject 父子关系和析构流程管理生命周期。
- 初始化阶段改为创建 `PerceptionPipeline::_tagConfig`，并配置截图间隔、PNG 编码、缓冲容量和缓冲开关。
- `MainWindow` 连接 `PerceptionPipeline::DataReady` 到 `OnPerceptionDataReady()`。
- `MainWindow` 连接 `PerceptionPipeline::ErrorOccurred` 到 `OnPerceptionError()`。
- 原 `OnScreenshotCaptured()` 改为 `OnPerceptionDataReady()`，继续负责发出 `PerceptionReceived` 并触发视觉 LLM 分析。
- 原 `OnScreenshotError()` 改为 `OnPerceptionError()`。
- `PerceptionPipeline` 增加 `GetLatestFrameSize()`，供 `MainWindow` 调用视觉 LLM 时获取最新帧尺寸。
- 视觉 LLM 请求限流逻辑保持不变，仍通过 `m_visionRequestInFlight` 避免并发截图分析请求堆积。

当前效果：

- `MainWindow` 不再直接依赖截图传感器实现，截图能力统一经由 `PerceptionPipeline` 输出。
- 原有 `PerceptionReceived` 信号和视觉 LLM 分析行为保持兼容。
- `PerceptionPipeline` 仍暂未接入 `AgentRuntime` 上下文，后续可继续迁移。

验证情况：

- 已执行 `git diff --check -- src/main_window.h src/main_window.cpp include/vpet/perception/perception_pipeline.h src/perception/perception_pipeline.cpp`。
- 结果仅提示仓库已有 LF/CRLF 换行差异，没有新增空白错误。
- 已执行 `cmake --build "F:\Pet Agent\build\opencode-screenshot"`。
- 构建通过，已成功链接 `VPet.exe`。

### 13. 文本基础处理下沉

本次开发根据设计反馈，调整了 DAG 执行边界：`input.parse` 和 `prompt.assemble` 不再作为 DAG 节点编排，而是移入 `AgentRuntime` 的固定基础处理流程。

修改文件：

- `include/vpet/agent/agent_runtime.h`
- `src/agent/agent_runtime.cpp`
- `agent_dag_structure.json`
- `agent_dag_structure.example.json`

实现内容：

- 删除 DAG 节点分发中的 `input.parse` 和 `prompt.assemble` 专用分支。
- 新增 `PrepareTextInputContext()`，在每次执行前统一准备文本输入上下文。
- 该基础处理固定执行，不再受 DAG 拓扑影响。
- 文本输入为空时会清理旧的 prompt / response / pending 状态，并记录输入不可用。
- 文本输入非空时会写入 `prompt.text`，供后续 `llm.chat` 节点直接使用。
- DAG 配置中移除了 `input.parse` 和 `prompt.assemble` 节点，仅保留可编排业务节点。
- 视觉输入节点 `vision.input` 仍保留，用于接收来自 `MainWindow` 的感知数据并写入运行时上下文。

当前效果：

- 文本基础预处理已经固定执行，不再依赖图结构。
- DAG 现在只表达真正需要编排的业务节点，例如视觉输入、LLM 调用和输出格式化。
- 这与“基础处理无论图如何都应执行”的预期一致。

验证情况：

- 已执行 `git diff --check -- include/vpet/agent/agent_runtime.h src/agent/agent_runtime.cpp agent_dag_structure.json agent_dag_structure.example.json`。
- 结果仅提示仓库已有 LF/CRLF 换行差异，没有新增空白错误。
- 已执行 `cmake --build "F:\Pet Agent\build\opencode-screenshot"`。
- 构建通过，已成功链接 `VPet.exe`。

### 14. DAG 拓扑序注册表执行

本次开发将 `AgentRuntime` 的节点执行从硬编码 `if` 分发改为处理器注册表驱动。

修改文件：

- `include/vpet/agent/agent_runtime.h`
- `src/agent/agent_runtime.cpp`

实现内容：

- 新增 `AgentRuntime::NodeHandler` 节点处理器类型。
- 新增 `RegisterNodeHandler()`，支持按节点 `type` 注册处理器。
- 新增 `m_nodeHandlers`，保存节点类型到处理器的映射。
- 新增 `RegisterDefaultNodeHandlers()`，集中注册内置节点处理器。
- `ExecuteNode()` 不再通过硬编码 `if (nodeType == ...)` 选择模块。
- `ExecuteNode()` 现在只负责校验节点、记录执行信息、按节点 `type` 查找处理器并执行。
- 未注册节点类型会返回明确错误，不再静默走 pass-through，避免配置错误被掩盖。
- DAG 的实际执行顺序仍完全来自 `AgentDagGraph::TopologicalSort()` 输出的拓扑序。

当前效果：

- 图结构负责决定执行顺序。
- 运行时注册表负责决定节点类型对应的执行逻辑。
- 后续新增节点类型时，只需要注册新的处理器，不需要继续修改 `ExecuteNode()` 的硬编码分支。

验证情况：

- 已执行 `grep` 检查 `agent_runtime.cpp` 中不再存在 `nodeType == ...` 分发判断。
- 已执行 `git diff --check -- include/vpet/agent/agent_runtime.h src/agent/agent_runtime.cpp`。
- 未发现新增空白格式错误。
- 已执行 `cmake --build "F:\Pet Agent\build\opencode-screenshot"`。
- 构建通过，已成功链接 `VPet.exe`。

### 15. 下一步计划

优先级建议：

1. 为 `llm.chat` 节点增加从 `config` 读取温度、最大 token 等请求参数。
2. 将 `output.format` 的结果明确接入气泡和 TTS 输出策略。
3. 将视觉模型配置改造成多模型列表配置，并让右键菜单从配置动态生成。
4. 为语音输入补充运行期验证，包括麦克风权限、GPT-SoVITS ASR 依赖和真实 LLM API Key。

## 2026-07-21

### 1. 情感输出节点接入 Agent DAG

本次开发新增情感化输出模块，并将其注册为可由 DAG 配置编排的节点。

新增文件：

- `include/vpet/agent/emotion_rewrite_node.h`
- `src/agent/emotion_rewrite_node.cpp`

修改文件：

- `CMakeLists.txt`
- `agent_dag_structure.json`
- `agent_dag_structure.example.json`
- `include/vpet/agent/agent_runtime.h`
- `src/agent/agent_runtime.cpp`

实现内容：

- 新增 `EmotionRewriteNode` 类。
- 新增节点类型 `emotion.rewrite`。
- 在 `AgentRuntime::RegisterDefaultNodeHandlers()` 中注册情感节点处理器。
- 默认 DAG 链路更新为：`vision.input -> llm.chat -> emotion.rewrite -> output.format`。
- 首轮没有对话历史时，情感节点直接透传原始 LLM 回复。
- 存在对话历史时，情感节点会基于上下文进行情感化输出处理。

当前效果：

- 情感模块已经成为独立节点，不再需要硬编码在输出节点中。
- DAG 配置可以选择插入或绕过 `emotion.rewrite`。

### 2. 情感模块升级为 LLM 情绪总结节点

本次开发将情感模块从关键词启发式改写升级为真正的 LLM 情绪总结节点。

修改文件：

- `include/vpet/agent/emotion_rewrite_node.h`
- `src/agent/emotion_rewrite_node.cpp`
- `include/vpet/agent/agent_runtime.h`
- `src/agent/agent_runtime.cpp`

实现内容：

- `emotion.rewrite` 在存在 `conversation.history` 时再次调用文本 LLM。
- 情感节点将最近 30 轮对话、当前用户输入和原始回复组成情感分析 prompt。
- 要求 LLM 返回严格 JSON，字段包含 `user_emotion`、`pet_emotion`、`rewrite`。
- 解析成功后写入 `emotion.user`、`emotion.pet`、`emotion.raw_response`、`emotion.output_text`。
- 情感节点返回异常 JSON 时会给出明确错误。
- 无上下文时保持原始回复透传，避免首轮额外调用 LLM。

当前效果：

- 情感模块不再依赖固定关键词规则。
- 回复风格可以基于最近上下文和当前用户情绪动态调整。

### 3. AgentRuntime 多阶段异步续跑

本次开发将原本只适配 `llm.chat` 的异步暂停机制泛化为运行时异步状态。

修改文件：

- `include/vpet/agent/agent_runtime.h`
- `src/agent/agent_runtime.cpp`

实现内容：

- 新增 `runtime.async.pending`、`runtime.async.pending_node_id`、`runtime.async.pending_node_type`、`runtime.async.pending_request_id`、`runtime.async.pending_resume_index`。
- `llm.chat` 发起 LLM 请求后写入统一异步等待状态。
- `emotion.rewrite` 发起 LLM 请求后复用同一异步等待状态。
- LLM 回调完成后，根据等待节点类型分派处理逻辑。
- `llm.chat` 返回后继续执行后续 DAG 节点。
- `emotion.rewrite` 返回后解析情感 JSON，并继续执行 `output.format`。

当前效果：

- 当前 DAG 支持 `llm.chat -> emotion.rewrite -> output.format` 的两阶段 LLM 异步流程。
- 后续新增异步节点可以继续复用 `runtime.async.*` 机制。

### 4. 对话历史维护

本次开发在最终输出节点中维护最近对话历史，为情感节点提供上下文。

修改文件：

- `include/vpet/agent/agent_runtime.h`
- `src/agent/agent_runtime.cpp`

实现内容：

- `output.format` 产出最终回复后写入 `conversation.history`。
- 历史格式为 `user: ...` 和 `assistant: ...`。
- 最多保留最近 30 轮对话，即 60 条记录。
- 每轮输入开始时清理上一轮临时输出状态，但保留历史上下文。

当前效果：

- 情感模块可以读取最近上下文进行 LLM 情绪总结。
- 后续记忆、画像、总结类节点也可以复用 `conversation.history`。

### 5. 节点互插语义别名桥

本次开发在现有 `AgentContext` 基础上增加轻量语义别名桥，提高节点互插兼容性。

修改文件：

- `include/vpet/agent/agent_runtime.h`
- `src/agent/agent_runtime.cpp`

实现内容：

- 新增跨节点语义层 key：`semantic.text.prompt`、`semantic.text.response`、`semantic.text.final`。
- 新增节点标准端口层 key：`node.input.prompt`、`node.input.text_response`、`node.output.text_response`、`node.output.text_final`。
- 新增 `PrepareNodeInputAliases()`，节点执行前自动补齐输入别名。
- 新增 `SyncNodeOutputAliases()`，节点执行后自动同步输出别名。
- `llm.last_response` 自动同步到 `semantic.text.response`。
- `emotion.output_text` 自动同步到 `semantic.text.response`。
- `output.text` 自动同步到 `semantic.text.final`。
- `output.format` 可直接读取 `semantic.text.response`，不再只依赖某个上游私有 key。

当前效果：

- `llm.chat -> output.format` 和 `llm.chat -> emotion.rewrite -> output.format` 均可通过语义层接通。
- 后续文本类节点只要遵守 `semantic.text.response`，即可更容易插入 DAG。

### 6. Agent Context key 规范化

本次开发将 Agent 上下文 key 从各个 `.cpp` 中抽出，集中到统一头文件，并编写项目协议文档。

新增文件：

- `include/vpet/agent/agent_context_keys.h`
- `AGENT_CONTEXT_KEY_PROTOCOL.md`

修改文件：

- `CMakeLists.txt`
- `src/agent/agent_context.cpp`
- `src/agent/agent_runtime.cpp`
- `src/agent/emotion_rewrite_node.cpp`

实现内容：

- 新增 `AgentContextKeys` 命名空间，统一定义 Agent 上下文 key 和内置节点类型 key。
- `AgentContext` 的 `user.input` 和 `runtime.executed_nodes` 改为引用统一 key。
- `AgentRuntime` 中的跨节点 key、运行时 key、视觉 key、输出 key 均改为引用统一 key。
- `EmotionRewriteNode` 中的情感 key、LLM key、异步 key 均改为引用统一 key。
- 清理 `src/agent/*.cpp` 中跨节点 key 的直接硬编码字符串。
- 新增 `AGENT_CONTEXT_KEY_PROTOCOL.md`，明确 key 命名分层和新增节点规则。

当前规范：

- `semantic.*`：跨节点语义数据层。
- `node.input.*` / `node.output.*`：节点标准端口层。
- `<module>.*`：节点或模块私有状态层。
- `runtime.*`：运行时内部状态层。

新增节点规则：

- 跨节点输入优先读取 `semantic.*` 或 `node.input.*`。
- 跨节点输出必须写入对应 `semantic.*`，或由运行时同步到 `semantic.*`。
- 不应直接依赖其他节点的私有 key。
- 新增 key 必须先加入 `include/vpet/agent/agent_context_keys.h`。

### 7. 验证情况

已执行语法检查：

- `src/agent/agent_context.cpp`
- `src/agent/agent_runtime.cpp`
- `src/agent/emotion_rewrite_node.cpp`

结果：

- 上述文件均通过 `g++ -fsyntax-only` 检查。
- 已检查 `src/agent/*.cpp`，跨节点 key 不再直接硬编码为 `QStringLiteral("llm.last_response")`、`QStringLiteral("semantic.*")`、`QStringLiteral("node.*")` 等字符串。

完整构建：

- 已执行 `E:\Qt\Tools\CMake_64\bin\cmake.exe --build build\Desktop_Qt_6_9_2_MinGW_64_bit-Debug`。
- 当前完整 CMake/Ninja 构建仍失败，但没有输出具体编译诊断。
- 失败覆盖 `mocs_compilation.cpp`、`main.cpp`、`main_window.cpp` 等旧文件，并非只发生在本次新增或修改的 Agent 文件上。
- 本轮仍以针对性语法检查作为 Agent 改动的有效验证。
