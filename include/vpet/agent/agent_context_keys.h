#ifndef VPET_AGENT_AGENT_CONTEXT_KEYS_H
#define VPET_AGENT_AGENT_CONTEXT_KEYS_H

#include <QString>

namespace vpet
{

/**
 * @brief Agent 上下文键与节点类型常量
 *
 * 跨节点数据交换必须优先使用 semantic.* 与 node.* 键；节点私有状态使用对应模块前缀。
 */
namespace AgentContextKeys
{

inline const QString CONVERSATION_HISTORY = QStringLiteral("conversation.history");
inline const QString EMOTION_LAST_REQUEST_ID = QStringLiteral("emotion.last_request_id");
inline const QString EMOTION_OUTPUT_TEXT = QStringLiteral("emotion.output_text");
inline const QString EMOTION_PET = QStringLiteral("emotion.pet");
inline const QString EMOTION_PROMPT_TEXT = QStringLiteral("emotion.prompt_text");
inline const QString EMOTION_RAW_RESPONSE = QStringLiteral("emotion.raw_response");
inline const QString EMOTION_SOURCE_TEXT = QStringLiteral("emotion.source_text");
inline const QString EMOTION_USER = QStringLiteral("emotion.user");
inline const QString EXECUTED_NODES = QStringLiteral("runtime.executed_nodes");
inline const QString INPUT_AVAILABLE = QStringLiteral("input.available");
inline const QString LLM_LAST_REQUEST_ID = QStringLiteral("llm.last_request_id");
inline const QString LLM_LAST_RESPONSE = QStringLiteral("llm.last_response");
inline const QString LLM_PENDING = QStringLiteral("llm.pending");
inline const QString NODE_INPUT_PROMPT = QStringLiteral("node.input.prompt");
inline const QString NODE_INPUT_TEXT_RESPONSE = QStringLiteral("node.input.text_response");
inline const QString NODE_OUTPUT_TEXT_FINAL = QStringLiteral("node.output.text_final");
inline const QString NODE_OUTPUT_TEXT_RESPONSE = QStringLiteral("node.output.text_response");
inline const QString OUTPUT_PENDING = QStringLiteral("output.pending");
inline const QString OUTPUT_TEXT = QStringLiteral("output.text");
inline const QString PROMPT_TEXT = QStringLiteral("prompt.text");
inline const QString RUNTIME_LAST_NODE_TYPE = QStringLiteral("runtime.last_node_type");
inline const QString RUNTIME_PASS_THROUGH_PREFIX = QStringLiteral("runtime.pass_through.");
inline const QString RUNTIME_PENDING = QStringLiteral("runtime.async.pending");
inline const QString RUNTIME_PENDING_NODE_ID = QStringLiteral("runtime.async.pending_node_id");
inline const QString RUNTIME_PENDING_NODE_TYPE = QStringLiteral("runtime.async.pending_node_type");
inline const QString RUNTIME_PENDING_REQUEST_ID = QStringLiteral("runtime.async.pending_request_id");
inline const QString RUNTIME_PENDING_RESUME_INDEX = QStringLiteral("runtime.async.pending_resume_index");
inline const QString SEMANTIC_TEXT_FINAL = QStringLiteral("semantic.text.final");
inline const QString SEMANTIC_TEXT_PROMPT = QStringLiteral("semantic.text.prompt");
inline const QString SEMANTIC_TEXT_RESPONSE = QStringLiteral("semantic.text.response");
inline const QString SEMANTIC_VISION_SUMMARY = QStringLiteral("semantic.vision.summary");
inline const QString USER_INPUT = QStringLiteral("user.input");
inline const QString VISION_ANALYSIS = QStringLiteral("vision.analysis");
inline const QString VISION_AVAILABLE = QStringLiteral("vision.available");
inline const QString VISION_INPUT_READY = QStringLiteral("vision.input_ready");
inline const QString VISION_LATEST_BASE64 = QStringLiteral("vision.latest_base64");
inline const QString VISION_LATEST_FRAME_ID = QStringLiteral("vision.latest_frame_id");
inline const QString VISION_LATEST_HEIGHT = QStringLiteral("vision.latest_height");
inline const QString VISION_LATEST_MODALITY = QStringLiteral("vision.latest_modality");
inline const QString VISION_LATEST_WIDTH = QStringLiteral("vision.latest_width");
inline const QString VISION_LLM_LAST_REQUEST_ID = QStringLiteral("vision.llm.last_request_id");
inline const QString VISION_LLM_PENDING = QStringLiteral("vision.llm.pending");
inline const QString VISION_UPDATED_AT = QStringLiteral("vision.updated_at");

inline const QString NODE_TYPE_EMOTION_REWRITE = QStringLiteral("emotion.rewrite");
inline const QString NODE_TYPE_LLM_CHAT = QStringLiteral("llm.chat");
inline const QString NODE_TYPE_OUTPUT_FORMAT = QStringLiteral("output.format");
inline const QString NODE_TYPE_VISION_INPUT = QStringLiteral("vision.input");
inline const QString NODE_TYPE_VISION_LLM = QStringLiteral("vision.llm");

} // namespace AgentContextKeys

} // namespace vpet

#endif // VPET_AGENT_AGENT_CONTEXT_KEYS_H
