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

## 2026-07-23 Agent Context 协议复查与补齐

### 1. 检查目标

本次检查对照 `AGENT_CONTEXT_KEY_PROTOCOL.md`，核对当前项目中的 Agent 上下文 key 和运行时执行流程是否严格遵守协议。

重点检查范围：

- `include/vpet/agent/agent_context_keys.h`
- `src/agent/agent_context.cpp`
- `src/agent/agent_runtime.cpp`
- `src/agent/emotion_rewrite_node.cpp`
- `agent_dag_structure.json`

### 2. 检查结论

整体结论：当前项目基本遵守协议，但异步回调路径此前不够严格。

已确认事项：

- Agent 上下文 key 已集中定义在 `AgentContextKeys` 命名空间。
- `src/agent/*.cpp` 未发现跨节点 key 以 `QStringLiteral("semantic.*")`、`QStringLiteral("node.*")`、`QStringLiteral("llm.last_response")` 等形式散落硬编码。
- 当前 DAG 节点类型使用 `vision.input`、`vision.llm`、`llm.chat`、`emotion.rewrite`、`output.format`，与内置节点类型常量一致。

发现的问题：

- `llm.chat` 与 `emotion.rewrite` 发起异步请求时，节点执行阶段尚无输出，运行时的 `SyncNodeOutputAliases` 无法同步最终结果。
- LLM 回调完成后此前主要写入 `llm.last_response`，情感改写回调完成后主要写入 `emotion.output_text`，未立即补齐 `semantic.text.response` 与 `node.output.text_response`。
- 这会让异步节点后接自定义语义节点时，不够严格依赖 `semantic.*` / `node.*` 协议层。

### 3. 运行时修复

修改文件：

- `src/agent/agent_runtime.cpp`

修复内容：

- `OnLlmChatCompleted()` 在普通 LLM 回调完成后同步：
- `llm.last_response -> semantic.text.response`
- `llm.last_response -> node.output.text_response`
- `OnLlmChatCompleted()` 在情感改写回调完成后同步：
- `emotion.output_text -> semantic.text.response`
- `emotion.output_text -> node.output.text_response`
- 无 DAG 续跑、直接输出 fallback 时同步：
- `output.text -> semantic.text.final`
- `output.text -> node.output.text_final`

修复后，异步节点不再只依赖私有 key 承载跨节点结果。

### 4. 协议文档更新

修改文件：

- `AGENT_CONTEXT_KEY_PROTOCOL.md`

更新内容：

- 命名分层从四层扩展为五层，补充 `user.*` / `input.*` 输入状态层。
- 补齐当前已存在的私有 key：`emotion.last_request_id`、`emotion.prompt_text`、`emotion.raw_response`、`emotion.source_text`、`vision.*`、`vision.llm.*`、`output.pending`。
- 补充 `runtime.pass_through.<node_id>` 运行时追踪 key。
- 明确新增节点必须优先产出 `semantic.*` 或 `node.output.*`。
- 明确视觉、多模态、记忆等新增语义数据应优先扩展 `semantic.*`。
- 补齐当前桥接关系，包含异步回调后必须补写的 `node.output.*` 和 `semantic.*`。

### 5. 验证情况

已执行静态搜索：

- 搜索 `semantic.*`、`node.input.*`、`node.output.*`、`llm.*`、`emotion.*`、`output.text`、`runtime.*` 等上下文 key 使用。
- 搜索 `.cpp` / `.h` / `.json` 中疑似硬编码 key 字符串。
- 复查 `agent_dag_structure.json` 中节点类型与常量表一致。

构建验证：

- 尝试执行 `cmake --build build\Desktop_Qt_6_9_2_MinGW_64_bit-Debug`，当前 shell 找不到 `cmake`。
- 尝试执行 `ninja -C build\Desktop_Qt_6_9_2_MinGW_64_bit-Debug`，当前 shell 找不到 `ninja`。
- 因此本轮未完成编译验证，结果以静态检查和最小代码审查为准。

## 2026-07-24 主动发话链路接通与可靠性修复

### 1. 背景

当前截图、视觉识别、气泡、TTS 两端能力已基本具备，但中间的主动发话决策与话题生成链路不完整，且存在本轮输入残留、输出来源错误、异步失败后 pending 不清理等问题。

本轮按推荐实施顺序推进前四项，并修复两个最高优先级可靠性问题。

### 2. 主动话题节点

新增文件：

- `include/vpet/agent/proactive_topic_node.h`
- `src/agent/proactive_topic_node.cpp`

修改文件：

- `agent_dag_structure.json`
- `agent_dag_structure.example.json`
- `include/vpet/agent/agent_context_keys.h`
- `include/vpet/agent/agent_runtime.h`
- `src/agent/agent_runtime.cpp`
- `AGENT_CONTEXT_KEY_PROTOCOL.md`

实现内容：

- 新增同步编排节点 `proactive.topic`。
- DAG 调整为：

```text
vision.input
→ vision.llm
→ proactive.topic
→ llm.chat
→ emotion.rewrite
→ output.format
```

- 节点读取 `semantic.vision.summary`，输出：
  - `semantic.proactive.should_speak`
  - `semantic.proactive.topic`
  - `semantic.proactive.reason`
  - `semantic.text.prompt`
  - `node.output.prompt`
- 已存在用户提示词时不覆盖，写入 `should_speak=false` 和 `reason=user_prompt_available`。
- 节点禁用、视觉摘要缺失或为空时静默结束。
- `output.format` 在 `should_speak=false` 且无文本输出时正常静默完成，不设置 `output.pending`。

当前限制：

- 第一版有视觉摘要即倾向于说话，尚未实现冷却、画面变化检测和话题去重。

### 3. 本轮输入残留与触发来源

修改文件：

- `include/vpet/agent/agent_context_keys.h`
- `src/agent/agent_runtime.cpp`
- `AGENT_CONTEXT_KEY_PROTOCOL.md`

实现内容：

- 新增 `runtime.trigger.type`，允许值为 `user` 或 `vision`。
- 用户入口写入 `user`，视觉帧入口写入 `vision`。
- `PrepareTextInputContext()` 根据触发类型判断本轮是否有用户输入，不再仅凭上下文中残留的 `user.input` 推断。
- 新增 `ClearInvocationInputState()`，在一轮完成或失败后清理：
  - `user.input`
  - `input.available`
  - `node.input.prompt`
  - `prompt.text`
  - `semantic.text.prompt`
  - `runtime.trigger.type`
- 视觉触发时主动清除本轮 `user.input`，避免旧用户输入污染主动发话链路。

### 4. 无用户输入的主动历史

修改文件：

- `src/agent/agent_runtime.cpp`

实现内容：

- `AppendConversationHistory()` 改为输出文本必填、用户输入可选。
- 用户回复：记录 `user: ...` + `assistant: ...`。
- 主动发话：只记录 `assistant: ...`，不伪造用户输入。

### 5. 最终输出来源 vision_proactive

修改文件：

- `include/vpet/agent/agent_context_keys.h`
- `include/vpet/agent/agent_runtime.h`
- `src/agent/agent_runtime.cpp`
- `src/main_window.h`
- `src/main_window.cpp`
- `AGENT_CONTEXT_KEY_PROTOCOL.md`

实现内容：

- 新增语义 key：`semantic.output.source`。
- 允许值：`user_response`、`vision_proactive`。
- `output.format` 根据 `runtime.trigger.type` 写入输出来源。
- 信号扩展为：

```cpp
AgentOutputReady(int requestId,
                 const QString &content,
                 const QString &source);
```

- `MainWindow::OnAgentOutputReady()` 映射：
  - `vision_proactive` → `SaySource::VisionProactive`
  - 其他 → `SaySource::UserResponse`
- 调用 `RequestSay()` 时检查返回值，拒绝时输出 warning，避免静默丢话。

### 6. 异步失败 pending 清理

修改文件：

- `include/vpet/agent/agent_runtime.h`
- `src/agent/agent_runtime.cpp`

实现内容：

- 新增 `ResetAsyncExecutionState()`，统一清理：
  - `llm.pending` / `vision.llm.pending`
  - `runtime.async.*`
  - 本轮输入与触发来源
  - 成员变量 `m_pendingResumeIndex` / `m_pendingNodeType` / `m_pendingRequestId`
- 覆盖路径：
  - LLM 空回复、无效 requestId、写上下文失败、续跑失败
  - Emotion 完成失败
  - Vision 空回复、写摘要失败、请求失败
  - `ExecuteFromIndex()` 节点执行失败
- requestId 与 pending 不匹配时只忽略响应，不清当前合法 pending，避免误伤在途请求。

### 7. 统一视觉 LLM 客户端

修改文件：

- `include/vpet/agent/agent_runtime.h`
- `src/agent/agent_runtime.cpp`
- `src/main_window.h`
- `src/main_window.cpp`

实现内容：

- 删除 MainWindow 侧闲置 `VisionLlmClient`、相关槽和 `m_visionRequestInFlight`。
- 视觉请求只由 `AgentRuntime` 内部客户端执行。
- 右键菜单模型切换改为调用：
  - `AgentRuntime::SetActiveVisionLlmProfile()`
  - `AgentRuntime::GetActiveVisionLlmProfile()`
- 视觉配置仍由 `main.cpp` 调用 `LoadDefaultVisionLlmConfig()` 加载。

### 8. 协议与进度

协议更新：

- 补充 `semantic.proactive.*`、`semantic.output.source`、`runtime.trigger.type`。
- 补充 `proactive.topic` 节点规则。
- 补充桥接：

```text
runtime.trigger.type -> semantic.output.source
semantic.output.source -> AgentOutputReady.source
semantic.vision.summary -> proactive.topic
proactive.topic -> semantic.text.prompt
```

推荐实施顺序进度：

1. 修复本轮 `user.input` 残留与触发来源判断：已完成
2. 新增 `proactive.topic` 节点和相关 key：基本完成
3. `output.format` 支持无用户输入主动输出：已完成
4. 最终输出附带 `vision_proactive` 来源：已完成
5. 异步失败 pending 清理：已完成
6. 冷却与智能 `should_speak`：未做
7. 画面变化检测：未做
8. 统一重复视觉客户端：已完成
9. 情绪标签接入桌宠动画：未做

### 9. 后续建议

- 在 `proactive.topic` 或前置策略中增加最短发话间隔、摘要去重、话题去重。
- 在截图入口增加本地画面变化检测，减少无效视觉请求。
- 补齐 `proactive.topic` 的 `SyncNodeOutputAliases` 桥接，避免只依赖节点内部双写。
- 评估 `AgentContext &GetContext()` 的封装暴露风险。
- 完成完整 CMake 编译验证。

### 10. P1 在线节点就绪队列

修改文件：

- `include/vpet/agent/agent_runtime.h`
- `src/agent/agent_runtime.cpp`
- `tests/agent_runtime_scheduler_test.cpp`
- `CMakeLists.txt`

实现内容：

- 新增私有 `_tagInvocationState`，保存本轮 `remainingInDegree`、稳定 FIFO `readyQueue` 和异步等待节点标识。
- `Execute()` 不再按加载期 `m_executionOrder` 逐项扫描；改为以 `AgentDagGraph::GetSourceNodes()` 初始化队列，并通过 `PumpReadyQueue()` 在线执行。
- 同步节点完成后调用 `CompleteNode()`：只对其直接后继递减运行时入度，入度变为零时才进入 ready queue。
- 异步节点保持 P1 的单 pending 限制：节点发起请求时暂停；回调完成后通过 `ResumePendingNode()` 标记该节点完成，再继续推进就绪节点。
- `Load()`、`Execute()` 与 `ExecuteWithUserInput()` 在 active invocation 或 pending 请求存在时明确拒绝，避免共享 `m_context` 被下一轮触发覆盖。
- 新增运行时调度测试：验证双源扇入节点在全部父节点完成后执行，并验证执行中拒绝重新执行或重载图。

当前约束：

- P1 仍使用单一共享 `AgentContext`，未实现 branch context、join merge 或 `pendingByRequestId`；这些能力属于后续 P2-P4。
- P1 将并列节点按节点声明顺序串行执行，消除了“先构造全局拓扑序再按索引恢复”的调度模型，但尚未提供并行数据隔离。

### 11. P2-P5 分支上下文、Join、异步恢复与跨轮调度

实现内容：

- P2：新增 session base、branch local、执行视图、增量和删除键，源分支与 fan-out 分支独立保存数据。
- P3：fan-in 节点合并父结果；不同值默认失败，并支持 `prefer_user`、`prefer_vision`、`concat`。
- P4：异步状态改为 `pendingByRequestId`，支持同一 invocation 多个异步分支乱序恢复，并处理失败、超时和晚到回调。
- P5：活动 invocation 期间的新触发保存为 FIFO 输入增量；当前轮结束后基于最新 session base 启动队首。
- 源节点声明 `config.trigger` 时按 user 或 vision 裁剪可达子图并重算子图入度；旧配置没有 trigger 声明时保持全图执行。
- MainWindow 不再丢弃活动异步轮次期间到达的视觉帧，而是交由 Runtime 排队。

验证：

- `agent_dag_graph_tests` 通过。
- `agent_runtime_scheduler_tests` 通过，覆盖 P1-P4 基线、FIFO 顺序和 trigger 子图裁剪。

P5 完善：

- 默认 DAG 升级为 `user.input` 与 `vision.input` 双 trigger source，共享后续文本生成和输出节点。
- `Start()` 仅加载没有 trigger 输入的启动配置，不再创建空 invocation。
- 活动 invocation 期间的视觉帧由 `UpdatePerceptionFrame()` 直接进入 FIFO，并兼容调用方随后调用 `Execute()`，不会重复排队。
- 新增 user 执行期间 vision 入队、失败后启动队首、队首读取最新 session history、跨轮 `user.input` 隔离测试。

### 12. P1-P4 阶段核验与 P4 多异步实现（2026-07-26）

本轮核验确认：P1、P2、P3 已完成；并继续完成 P4。P5 尚未作为本轮实现目标开始，后续应以实际代码和测试为准推进，避免沿用未验证的进度描述。

P1 在线调度：

- 使用 `readyQueue` 在线推进，不再依赖加载期拓扑序作为运行时恢复计划。
- 节点只有在真正完成后才会递减后继入度；后继入度归零后才进入就绪队列。
- 同层节点按声明顺序稳定执行，节点失败不会继续调度其后继。

P2 分支上下文：

- 使用 `m_sessionContext` 作为跨轮基座，使用 branch local 保存本轮可写增量。
- 执行视图支持快照、delta 和删除键记录。
- source 分支与 fan-out 分支互相隔离，失败 invocation 的输出不会写入 session base。

P3 Join 合并：

- fan-in 节点等待所有父节点完成后创建 join branch。
- 单一来源键直接复制；多个父节点提供相同值时保留。
- 不同值、删除与写入冲突默认失败，错误包含 join 节点、冲突键和父节点。
- 支持 `prefer_user`、`prefer_vision`、`concat`，未知策略明确拒绝。
- `runtime.executed_nodes` 在 join 处按父节点声明顺序去重合并；其他运行时控制键不跨 join 传播。

P4 多异步恢复：

- 删除 `m_pendingRequestId`、`m_pendingNodeType`、`pendingNodeId` 和 resume index 单槽恢复模型。
- 在 invocation 内使用 `pendingByRequestId` 保存 request ID、invocation ID、node ID、branch ID、节点类型和独立 continuation context。
- 一个 invocation 可以同时挂起多个异步节点；无关 ready branch 不会被单个 pending 阻塞。
- 回调按 request ID 定位对应分支，支持乱序恢复；未知、重复和晚到回调不会污染当前 invocation。
- 已知异步请求失败或超时会终止所属 invocation，并清理全部 pending，不提交本轮 session 状态。
- 支持节点配置 `config.async_timeout_ms`，默认 120000 毫秒，有效范围为 1 到 600000 毫秒。

测试与构建验证：

- `VPet` 完整构建成功。
- `agent_dag_graph_tests` 通过。
- `agent_runtime_scheduler_tests` 通过，覆盖 P1-P4 基线、Join 冲突策略、双异步乱序、异步失败、异步超时和晚到回调。
- CTest：2/2 测试目标通过。
- 测试构建目录 `build-tests/` 已清理。

本轮状态校正与后续补齐：

- P5 已在上一轮完成：跨轮 invocation FIFO、user/vision trigger 子图裁剪、输出与 conversation history 的 invocation 级隔离均已有代码和测试覆盖。
- 本轮新增主动发话基础抑制：`min_interval_ms` 冷却、`dedup_window_ms` 摘要去重和持久化最近主动输出状态。
- 本轮新增视觉帧内容 hash 去重；相同编码内容不会重复进入 Runtime。
- 本轮将视觉等待队列调整为 latest-wins，用户输入仍保持 FIFO。
- 本轮新增 `InvocationQueuePolicy` 和 `AgentOutputPolicy`，从 `AgentRuntime` 抽离跨轮队列与主动输出状态职责。
- 本轮新增应用入口级 Runtime 集成测试，覆盖用户输入、视觉帧写入和重复帧过滤。

## 2026-07-26 主动策略、队列策略与文档完善

### 1. 主动发话抑制策略落地

本次将 `proactive.topic` 从“有摘要即发话”升级为带抑制策略的节点。

新增/修改内容：

- `proactive_topic_node.cpp` 新增 `IsSpeechAllowed`：
  - 读取 `min_interval_ms`（默认 30000ms）
  - 读取 `dedup_window_ms`（默认 300000ms）
  - 使用 SHA-256 计算视觉摘要指纹
  - 检查 `proactive.last_spoken_at` 实现冷却
  - 检查 `proactive.last_summary_hash` 实现相同摘要抑制
- `output.format` 成功视觉主动输出后，持久化：
  - `proactive.last_spoken_at`
  - `proactive.last_summary_hash`
  - `semantic.vision.frame_hash`
- 默认 DAG 配置和示例配置均补充了 `min_interval_ms` / `dedup_window_ms`。
- 抑制原因明确写入 `semantic.proactive.reason`（cooldown / duplicate_summary 等）。

当前效果：

- 视觉主动发话不再无限制触发。
- 相同画面摘要在冷却窗口内会被跳过。
- 抑制决策对节点外部可见，便于后续 UI/日志展示。

### 2. 视觉帧去重 + latest-wins 队列策略

解决静止画面反复截图导致的重复视觉请求问题。

实现内容：

- `AgentRuntime::UpdatePerceptionFrame` 计算输入帧 SHA-256 内容指纹。
- 相同指纹直接跳过，不更新上下文、不触发执行。
- 引入 `m_lastPerceptionFrameHash` 作为运行时级最近接受帧缓存。
- 新增 `InvocationQueuePolicy` 类，独立管理跨轮排队：
  - 用户输入：严格 FIFO
  - 视觉触发：latest-wins（同类等待项直接替换）
- `AgentRuntime` 原有队列逻辑迁移到该策略类。
- `CommitInvocationResult` 会把持久化 key（`proactive.*`、`semantic.vision.frame_hash`）提交到 session base。

验证：

- 重复视觉帧不会产生新 invocation。
- 用户输入与视觉输入在活动轮期间可同时排队，互不覆盖。

### 3. Runtime 文件拆分与策略模块化

为后续继续拆分打基础。

新增文件：

- `include/vpet/agent/invocation_queue_policy.h`
- `src/agent/invocation_queue_policy.cpp`
- `include/vpet/agent/agent_output_policy.h`
- `src/agent/agent_output_policy.cpp`

改动：

- `AgentRuntime` 仅保留调度、异步、Join、节点分发等核心逻辑。
- 跨轮队列策略和主动输出持久化逻辑已抽离为独立可测试类。
- `CMakeLists.txt` 同步更新所有测试目标和主程序链接。

### 4. 应用级集成测试补齐

新增文件：

- `tests/application_integration_test.cpp`

新增 CMake 测试目标：`application_integration_tests`

覆盖场景：

- 用户输入完整走 `user.input → llm.chat → output.format` 并得到最终输出。
- 视觉帧写入后 `semantic.*` / 私有 key 正确填充。
- 连续相同视觉帧被去重，第二帧不覆盖上下文。

验证结果：

- 在正确 64 位 MinGW + Qt 环境下：
  - `agent_dag_graph_tests` 通过
  - `agent_runtime_scheduler_tests` 通过（含新增抑制与去重测试）
  - `application_integration_tests` 通过
- CTest 3/3 通过。

### 5. README 补充 DAG 修改方式与可用模块

大幅扩充“Agent DAG”一节：

- 新增“DAG 修改方式”小节，包含完整 JSON 示例、节点/边规则、trigger 裁剪说明、Join 冲突行为。
- 列出 7 个当前可用模块及输入/输出/配置要点：
  - `user.input`
  - `vision.input`
  - `vision.llm`
  - `proactive.topic`
  - `llm.chat`
  - `emotion.rewrite`
  - `output.format`
- 给出 `proactive.topic` 完整配置示例和抑制原因说明。
- 新增“节点插入原则”小节，强调必须使用 `semantic.*` 公共层。
- 修正描述：用户输入 FIFO、视觉输入 latest-wins + 内容 hash 去重。
- 明确 `llm_config.json` 是实际运行配置，`llm_config.example.json` 仅为模板。

同步更新其他文档（协议、架构说明、构想.md）中的对应表述。

### 6. 配置与工程规范修正

- 项目根目录新增 `llm_config.json`（从示例模板落地）。
- `.gitignore` 新增 `llm_config.json` 规则，防止真实 API Key 入库。
- 确认 `AgentRuntime::FindDefaultLlmConfigPath` 及启动流程只查找 `llm_config.json`。

### 7. 验证与发布情况

- 使用 E:\Qt\6.9.2\mingw_64 + E:\Qt\Tools\mingw1310_64 完整 Ninja 构建成功。
- 所有测试目标链接正常，运行时不出现 DLL 架构错误。
- `git diff --check` 通过。
- 变更已提交并推送至 `origin/ClaudeCode` 分支（commit 49ae586）。

当前状态总结：

- Agent 可靠性基础已具备（抑制 + 去重 + 排队策略）。
- 文档已能指导用户直接修改 DAG 并理解每个模块作用。
- 文本 LLM 配置已切换为标准 `llm_config.json`。
- Runtime 开始模块化，但主文件仍较大，后续可继续拆分异步恢复、Join、节点执行等部分。

后续建议（更新后）：

1. 继续补齐用户忙碌、TTS 播放中、专注模式的主动抑制策略。
2. 在感知管道层增加感知级画面变化检测（不只内容 hash）。
3. 把 `llm.chat` 的温度、max_tokens 等参数暴露到节点 config。
4. 完善情绪标签到动画状态的映射。
5. 增加更多端到端应用场景测试（带真实 LLM + TTS 的冒烟测试）。

## 2026-07-27 外部评审建议整改与状态核验

本轮依据 `来自fable5的评判及项目建议.md` 对安全、隐私、生命周期、Agent 输出契约和 UI 性能问题进行整改，并按当前工作树重新核验实施状态。

### 1. 本周高风险整改已落地

- 截图感知改为默认关闭，只有用户通过右键菜单主动开启后才启动；运行期间显示红色指示点，并明确提示画面将发送到外部 API。
- 移除栈上 `TtsServerManager` 与 `MainWindow` 的 QObject 父子关系，修复退出时可能发生的重复析构和堆损坏。
- 语音输入停止录音后不再立即启动 ASR；改为等待 `QMediaRecorder::StoppedState`，确保 WAV 写入完成。
- `AgentOutputReady` 统一移到 executor 的 invocation 完成回调发射，使纯同步 DAG、Vision 终止 DAG 和默认异步链都遵守同一输出契约。
- 新增纯同步终止图的输出信号回归测试，并在默认链测试中断言最终输出。
- `PetController` 只在帧路径或气泡内容实际变化时发射信号；`MainWindow` 使用 `QPixmapCache` 缓存缩放后的动画帧。
- 缺少 Say 动画资源时直接显示文本气泡并清理等待状态，避免回答丢失、无限重排队和逐帧日志刷屏。
- 修复模型菜单 `QActionGroup` 生命周期不足导致单选互斥失效的问题。

### 2. Runtime 模块化进展

新增并从 `agent_runtime.cpp` 中拆出：

- `AgentGraphExecutor`
- `AgentNodeRegistry`
- `AgentAsyncBridge`

上述模块已经加入 CMake 主程序和测试目标。Runtime 主文件仍保留 vision/input、vision/LLM、LLM chat 和 output format 等节点实现，后续仍需继续拆分。

### 3. 工程目录整理进展

- `CMakeLists.txt.user` 已从跟踪内容中删除，并加入 `.gitignore`。
- 新增 `.gitattributes`，统一文本换行规则并将 PNG 标记为二进制文件。
- 根目录手工测试驱动迁移到 `tests/manual/`。

以上工程目录整理内容已纳入本轮整改提交范围。

### 4. P0/P1 状态

P0 尚未完全清零：

- 根目录本地 `llm_config.json` 仍包含已外传的真实 DeepSeek API Key。虽然文件被 `.gitignore` 排除且 Git 历史中未发现该 Key，但旧 Key 必须在服务商控制台吊销和轮换后才能关闭此 P0。
- 新 Key 后续应从环境变量、Windows Credential Manager 或项目目录外的 `%APPDATA%` 配置读取，不应继续保存在项目目录。

剩余主要 P1：

- 动画加载尚未校验必需 Idle/Nomal 资源。

### 5. 验证状态

- 已完成当前工作树的源码级逐项核验。
- 使用 `E:\Qt\6.9.2\mingw_64`、`E:\Qt\Tools\mingw1310_64`、CMake 和 Ninja 创建独立 Debug 构建目录 `build/verification-qt6.9.2-mingw-path`。
- 当前工作树全量构建成功，`VPet.exe`、`agent_dag_graph_tests.exe`、`agent_runtime_scheduler_tests.exe` 和 `application_integration_tests.exe` 均成功链接。
- CTest 3/3 通过，0 个测试失败：
  - `agent_dag_graph_tests` 通过。
  - `agent_runtime_scheduler_tests` 通过。
  - `application_integration_tests` 通过。
- 首次直接调用 MinGW 编译器时因工具链目录未加入 `PATH` 而无法启动；补充 MinGW、Qt、CMake 和 Ninja 的 `bin` 路径后，干净配置、构建和测试全部通过。该问题属于命令行环境配置，不是源码或测试失败。

### 6. 下一步顺序

1. 立即吊销并轮换已暴露的 DeepSeek Key，完成 P0 闭环。
2. 校验动画必需资源并为缺失资源提供明确错误。
3. 继续关闭评审中剩余的 P1/P2 项。

## 2026-07-28 敏感日志、MiMo 响应与 Voice 临时文件整改

### 1. 敏感日志默认脱敏

- Vision LLM 不再输出完整响应 JSON；成功和失败路径只记录 request ID、HTTP 状态和响应字节数。
- Text LLM 的非 2xx 错误不再把服务端响应正文写入错误消息。
- TTS 不再记录参考音频路径、prompt text、待合成文本、请求 JSON、服务端错误正文及临时音频路径；改为记录状态、字符数和字节数。
- Voice Input 和 Agent UI 回调不再输出转写正文、模型回复或最终回答，只记录字符数和请求元数据。
- GPT-SoVITS 进程异常退出时只记录 stderr 字节数，不记录可能包含用户文本或本地路径的正文。

### 2. MiMo 响应字段兼容

- MiMo 响应优先读取标准 `choices[0].message.content`。
- 仅当 `content` 去除空白后为空时，才回退读取 `reasoning_content`。
- GPT 和 MiMo 的提取结果统一执行首尾空白清理。

### 3. Voice 临时文件生命周期收口

- `VoiceInputManager` 显式保存当前 session 临时目录，并由同一类负责创建和释放。
- ASR 成功、ASR 失败、输出读取失败、ASR 启动失败、录音器错误和析构路径均调用统一清理函数。
- 清理同时删除 WAV、ASR 输出和 session 子目录，并清空所有关联路径状态。
- 删除失败时保留 session 路径，避免丢失后续重试清理的能力；下一次录音开始前必须先成功清理上一轮目录。
- ASR 进程失败消息只保留退出码和 stdout/stderr 字节数，不再传播进程正文。

### 4. 验证结果

- Qt 6.9.2 + MinGW 13.1 Debug 增量构建成功，`VPet.exe` 和三个测试目标均成功链接。
- CTest 3/3 通过，0 个测试失败。
- 敏感正文日志静态扫描未再发现 Vision/TTS/Voice/Agent 正文输出。
- `git diff --check` 未发现空白错误，仅有工作树既有的 LF/CRLF 转换提示。
- MiMo 字段选择函数目前是私有静态实现，现有测试未提供 HTTP reply 注入接口；本轮通过源码分支复核和全量回归测试验证，后续可在 LLM 客户端测试解耦时补专门单元测试。

## 2026-07-28 TTS 端口清理与 Ready 后崩溃修复

### 1. TTS 进程所有权收敛

修改文件：

- `include/vpet/tts_server_manager.h`
- `src/tts_server_manager.cpp`

修复内容：

- 删除启动前通过 PowerShell 查询并强制终止 `9880` 端口占用进程的逻辑。
- 删除端口清理后的主线程固定休眠，避免 TTS 启动阶段同步阻塞 UI。
- `TtsServerManager` 只停止自身 `QProcess` 启动并持有的 GPT-SoVITS 进程，不再影响其他应用或用户手工启动的服务。
- TTS 子进程启动失败时立即释放 `QProcess`，允许后续重新调用 `Start()`。
- `Stop()` 主动断开进程回调后再终止当前子进程，避免正常退出被误报为服务崩溃。

### 2. Ready 后异常退出处理

修复内容：

- `OnProcessFinished()` 不再因服务已经 Ready 而跳过退出处理。
- 子进程退出后统一停止并释放健康检查定时器、将 `m_isReady` 复位为 `false`、释放进程对象。
- Ready 后退出会发出明确的 `StatusChanged` 和 `ServerStartFailed`，错误信息包含退出码。
- `errorOccurred` 的终止类错误交由 `finished` 统一收口，避免重复报告，并保留退出前 Ready 状态用于准确诊断。
- 健康检查回调在进程已退出或对象已释放时不会再把服务错误标记为 Ready。
- 异常退出清理完成后，管理器可再次启动新的 TTS 子进程。

当前效果：

- VPet 不再强杀任意占用 `9880` 端口的进程。
- TTS 服务 Ready 后崩溃会立即撤销就绪状态并报告失败。
- 正常关闭只回收本应用持有的 TTS 子进程，不产生意外退出告警。

### 3. 验证情况

- 使用 Qt 6.9.2、MinGW 13.1 和 CMake 完成 `VPet.exe` 全量增量构建，构建成功。
- 执行 CTest，`agent_dag_graph_tests`、`agent_runtime_scheduler_tests`、`application_integration_tests` 全部通过。
- CTest 结果：3/3 通过，0 失败。
- `git diff --check` 未发现本轮新增空白格式错误；仅有仓库换行转换提示。

### 4. 提交前审查修复

- TTS 同步启动失败时不再进入 60 秒等待循环。
- 健康检查超时会立即停止并回收本应用启动的 Python 子进程。
- Agent invocation 完成回调改在清理 trigger 前执行；自定义视觉终止图未显式写入 output source 时，会依据本轮 vision trigger 返回 `vision_proactive`。
- 同步终止图回归测试改为真实视觉触发，并验证无需节点手工写 source 也能得到正确来源。
- 修复后使用 Qt 6.9.2 + MinGW 13.1 完成增量构建，CTest 3/3 通过。

---

（日志末尾）
