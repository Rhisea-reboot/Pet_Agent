# Agent Context Key Protocol

本文档定义 Agent DAG 节点通过 `AgentContext` 交换数据时必须遵守的 key 命名协议。新增节点应优先使用本文档中的语义层 key，避免直接依赖其他节点的私有实现 key。

## 命名分层

Agent 上下文 key 分为四层：

1. `semantic.*`：跨节点语义数据层，用于节点互插和自动兼容。
2. `node.input.*` / `node.output.*`：当前节点标准端口层，用于运行时桥接输入输出。
3. `<module>.*`：节点或模块私有状态层，例如 `llm.*`、`emotion.*`、`vision.*`。
4. `runtime.*`：AgentRuntime 内部状态层，不应被业务节点当作业务输入使用。

## 跨节点语义层

新增节点的跨节点输入输出应优先使用以下 key：

| Key | 含义 | 推荐用途 |
| --- | --- | --- |
| `semantic.text.prompt` | 用户或上游组装后的提示词 | LLM、改写、路由节点的输入 |
| `semantic.text.response` | 中间回复文本 | LLM 输出、过滤、改写、情感模块输出 |
| `semantic.text.final` | 最终回复文本 | UI、TTS、日志和最终输出 |

后续新增多模态或记忆节点时，按同一格式扩展：

```text
semantic.image.base64
semantic.vision.state
semantic.memory.retrieval
semantic.emotion.user
semantic.emotion.pet
```

## 节点标准端口层

运行时会把语义层数据桥接到节点标准端口：

| Key | 含义 |
| --- | --- |
| `node.input.prompt` | 当前节点可直接读取的提示词输入 |
| `node.input.text_response` | 当前节点可直接读取的回复文本输入 |
| `node.output.text_response` | 当前节点产出的中间回复文本 |
| `node.output.text_final` | 当前节点产出的最终回复文本 |

节点实现可以读取 `node.input.*`，但跨节点协议仍以 `semantic.*` 为准。

## 模块私有状态层

模块私有 key 只用于兼容旧实现、调试或模块内部状态，不应作为新节点互插接口。

当前保留的私有 key：

| Key | 所属模块 | 用途 |
| --- | --- | --- |
| `llm.last_response` | `llm.chat` | LLM 原始回复，兼容旧节点 |
| `llm.last_request_id` | `llm.chat` | 最近一次 LLM 请求 ID |
| `llm.pending` | `llm.chat` | LLM 请求等待状态 |
| `emotion.output_text` | `emotion.rewrite` | 情感节点改写结果 |
| `emotion.user` | `emotion.rewrite` | 用户情绪标签 |
| `emotion.pet` | `emotion.rewrite` | 桌宠情绪标签 |
| `output.text` | `output.format` | 最终输出文本，兼容 UI 层 |
| `conversation.history` | conversation | 最近对话历史 |

## 运行时状态层

`runtime.*` key 由 `AgentRuntime` 维护，业务节点不得依赖这些 key 做业务判断。

| Key | 用途 |
| --- | --- |
| `runtime.executed_nodes` | 已执行节点列表 |
| `runtime.last_node_type` | 最近执行节点类型 |
| `runtime.async.pending` | 是否存在异步节点等待回调 |
| `runtime.async.pending_node_id` | 等待回调的节点 ID |
| `runtime.async.pending_node_type` | 等待回调的节点类型 |
| `runtime.async.pending_request_id` | 等待回调的请求 ID |
| `runtime.async.pending_resume_index` | 回调后续跑的拓扑序下标 |

## 新增节点规则

新增节点必须遵守以下规则：

1. 跨节点输入优先读取 `semantic.*` 或 `node.input.*`。
2. 跨节点输出必须写入对应的 `semantic.*` 或由运行时同步到 `semantic.*`。
3. 不要让节点直接依赖某个特定上游节点的私有 key，例如只读 `llm.last_response`。
4. 私有 key 使用 `<module>.<field>` 命名，例如 `memory.retrieved_text`。
5. 异步状态统一使用 `runtime.async.*`，不得新增并行的异步状态协议。
6. 新增 key 必须先加入 `include/vpet/agent/agent_context_keys.h`，不得在 `.cpp` 中散落硬编码字符串。

## 当前桥接关系

运行时当前维护以下桥接：

```text
prompt.text -> semantic.text.prompt
semantic.text.prompt -> prompt.text
llm.last_response -> semantic.text.response
semantic.text.response -> llm.last_response
emotion.output_text -> semantic.text.response
semantic.text.response -> node.input.text_response
output.text -> semantic.text.final
semantic.text.final -> node.output.text_final
```

这意味着逻辑兼容的文本节点可以通过 `semantic.text.response` 互插。
