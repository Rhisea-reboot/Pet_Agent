# VPet / Pet Agent

基于 Qt 6 的 Windows 桌面宠物，支持动画交互、语音输入、屏幕视觉感知、DAG 编排的 LLM Agent，以及 TTS 语音播报。

目标：从“会动的桌宠”升级为“能看屏幕、能听你说话、能主动搭话”的陪伴式 Agent。

---

## 功能概览

| 能力 | 说明 | 状态 |
|------|------|------|
| 桌宠动画 | PNG 序列帧、ABC 三段式动作、待机/点击/拖拽等 | 可用 |
| 气泡与说话 | 聊天气泡窗口、说话队列与优先级 | 可用 |
| 语音输入 | 全局热键录音转写，提交到 Agent | 可用 |
| 屏幕感知 | 定时截图 → 编码 → Agent 上下文 | 可用 |
| 视觉 LLM | 截图理解，生成画面摘要 | 可用 |
| 主动话题 | `proactive.topic` 根据视觉摘要和策略决定是否发话 | 可用，含冷却与摘要去重 |
| 文本 LLM | 用户回复 / 主动话语生成 | 可用（需配置） |
| 情感改写 | 对话上下文下的情感标签与改写 | 部分可用 |
| TTS | GPT-SoVITS HTTP 合成与播放 | 可用（需本地服务） |

尚未完善：情绪驱动动画、生产级打扰控制、真实服务端端到端测试。

---

## 架构

```text
截图 / 语音
    │
    ▼
PerceptionPipeline / VoiceInputManager
    │
    ▼
AgentRuntime（DAG）
    user.input ───────────────────────┐
                                      ▼
    vision.input → vision.llm → proactive.topic → llm.chat
                                                        → emotion.rewrite → output.format
    │
    ▼
MainWindow → PetController::RequestSay(source)
    │
    ▼
TTS 合成 → 气泡 + 音频 + 说话动画
```

Agent 节点通过 `AgentContext` 交换数据，key 协议见 [AGENT_CONTEXT_KEY_PROTOCOL.md](AGENT_CONTEXT_KEY_PROTOCOL.md)。

当前默认 DAG（[agent_dag_structure.json](agent_dag_structure.json)）：

```text
vision.input
→ vision.llm
→ proactive.topic
→ llm.chat
→ emotion.rewrite
→ output.format
```

触发来源：

- `runtime.trigger.type = user`：语音/文本输入
- `runtime.trigger.type = vision`：截图主动感知

最终输出携带 `semantic.output.source`：

- `user_response` → `SaySource::UserResponse`
- `vision_proactive` → `SaySource::VisionProactive`

---

## 目录结构

```text
Pet Agent/
├── Animation/                 # 桌宠动画资源
├── include/vpet/              # 公共头文件
│   ├── agent/                 # DAG 运行时、节点、上下文 key
│   ├── llm/                   # 文本/视觉 LLM 客户端
│   ├── perception/            # 帧缓冲、编码、感知管道
│   ├── sensor/                # 截图传感器
│   └── speech/                # 语音输入
├── src/                       # 实现与 MainWindow
├── GPT-SoVITS/                # 本地 TTS 服务（可选）
├── docs/                      # 设计与集成文档
├── agent_dag_structure.json   # Agent DAG 配置
├── llm_config.example.json    # 文本 LLM 配置模板
├── vision_llm_config.example.json
├── tts_config.json            # TTS 服务配置
└── CMakeLists.txt
```

---

## 环境要求

- Windows 10/11
- C++17 编译器（建议 MinGW 或 MSVC，与 Qt 套件一致）
- CMake ≥ 3.16
- Qt 6（`Core` / `Gui` / `Widgets` / `Network` / `Multimedia`）
- 可选：Python 环境 + GPT-SoVITS（TTS）
- 可选：OpenAI 兼容的文本/视觉 LLM API

---

## 构建

```bash
# 在项目根目录
cmake -S . -B build -DCMAKE_PREFIX_PATH="E:/Qt/6.x.x/mingw_64"
cmake --build build --config Debug
```

用 Qt Creator 打开 `CMakeLists.txt` 亦可。可执行目标名：`VPet`。

运行时需能找到：

1. `Animation/` 动画目录（可执行文件旁，或项目根目录）
2. `agent_dag_structure.json`
3. `llm_config.json`（文本对话/主动发话）
4. `vision_llm_config.json`（截图理解）
5. `tts_config.json`（TTS，可选）

配置查找路径通常包括：可执行文件目录、当前工作目录、上级目录。

---

## 配置

### 1. 文本 LLM

```bash
copy llm_config.example.json llm_config.json
```

编辑 `llm_config.json`：

```json
{
  "base_url": "https://api.openai.com/v1",
  "api_key": "YOUR_API_KEY",
  "model": "gpt-4o-mini",
  "timeout_ms": 30000
}
```

支持 OpenAI 兼容接口（改 `base_url` / `model` 即可）。

### 2. 视觉 LLM

```bash
copy vision_llm_config.example.json vision_llm_config.json
```

编辑 API Key、模型与 `default_prompt`。右键桌宠可切换图像识别模型档位（`mimo-v2.5` / `gpt`），切换作用于 `AgentRuntime` 内部视觉客户端。

### 3. Agent DAG

默认使用根目录 `agent_dag_structure.json`。可参考 `agent_dag_structure.example.json` 调整节点与边。

默认 DAG 使用两个 trigger source：`user.input` 声明 `trigger=user`，`vision.input` 声明 `trigger=vision`。Runtime 会按触发来源裁剪可达子图；活动 invocation 期间用户输入进入 FIFO，视觉输入采用 latest-wins 替换等待队列中的旧帧。

### 4. TTS

编辑 `tts_config.json`（服务地址、参考音频、语种等）。启动时会尝试拉起本地 GPT-SoVITS；失败时桌宠仍可运行，说话可能退化为仅文字气泡。

---

## 使用

1. 配置 `llm_config.json`、`vision_llm_config.json`（及可选 TTS）
2. 启动 `VPet`
3. 桌宠出现在屏幕上：
   - **左键拖动**：提起/移动
   - **点击头/身体**：触摸动画
   - **右键菜单**：语音输入说明、视觉模型切换
   - **Ctrl+Alt+V**：开始/结束语音输入并提交给 Agent
4. 截图感知默认约每 3 秒一帧；运行时空闲时立即进入视觉 DAG，已有 invocation 时按 FIFO 等待执行

---

## Agent 数据约定（摘要）

跨节点优先使用语义层 key：

| Key | 含义 |
|-----|------|
| `semantic.vision.summary` | 画面摘要 |
| `semantic.proactive.should_speak` | 是否主动发话 |
| `semantic.text.prompt` | 文本 LLM 提示词 |
| `semantic.text.response` | 中间回复 |
| `semantic.text.final` | 最终回复 |
| `semantic.output.source` | `user_response` / `vision_proactive` |
| `runtime.trigger.type` | `user` / `vision` |

一轮结束后会清理本轮 `user.input`、提示词端口和触发类型；`conversation.history` 持久保留。主动发话历史可只记录 `assistant:`，不伪造用户输入。

完整协议：[AGENT_CONTEXT_KEY_PROTOCOL.md](AGENT_CONTEXT_KEY_PROTOCOL.md)

---

## 当前限制

- 主动策略目前提供固定冷却和摘要指纹去重，尚未接入用户忙碌、语音播放和专注模式
- 视觉帧按编码内容 hash 去重，但尚未实现感知级相似度检测
- 情感标签尚未驱动桌宠动画状态
- 文本 LLM 仓库仅提供 `llm_config.example.json`，需自行落地配置
- `FRAMEWORK.md` 中的完整 `IModule`/`Agent` 模块中心仍在演进，当前主路径是 `MainWindow` + `AgentRuntime`

---

## 相关文档

| 文档 | 内容 |
|------|------|
| [AGENT_CONTEXT_KEY_PROTOCOL.md](AGENT_CONTEXT_KEY_PROTOCOL.md) | Agent 上下文 key 协议 |
| [FRAMEWORK.md](FRAMEWORK.md) | 视觉感知框架设计 |
| [DEVELOPMENT_LOG.md](DEVELOPMENT_LOG.md) | 开发日志 |
| [vpet.md](vpet.md) | 动画与交互设计 |
| [Structure.md](Structure.md) | Agent DAG 设计理念 |
| [docs/tts_integration_plan.md](docs/tts_integration_plan.md) | TTS 集成说明 |

---

## 许可证

以仓库内实际声明为准。第三方组件（如 GPT-SoVITS、Qt）遵循各自许可证。
