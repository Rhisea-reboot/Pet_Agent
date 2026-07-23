#include "vpet/agent/agent_runtime.h"
#include "vpet/agent/agent_context_keys.h"
#include "vpet/agent/emotion_rewrite_node.h"
#include "vpet/llm/llm_client.h"
#include "vpet/llm/vision_llm_client.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSize>
#include <QStringList>
#include <QVariant>

namespace vpet
{

namespace
{

const QString LLM_CONFIG_FILE_NAME = QStringLiteral("llm_config.json");
const QString VISION_LLM_CONFIG_FILE_NAME = QStringLiteral("vision_llm_config.json");
const QString &NODE_TYPE_VISION_INPUT = AgentContextKeys::NODE_TYPE_VISION_INPUT;
const QString &NODE_TYPE_VISION_LLM = AgentContextKeys::NODE_TYPE_VISION_LLM;
const QString &NODE_TYPE_LLM_CHAT = AgentContextKeys::NODE_TYPE_LLM_CHAT;
const QString &NODE_TYPE_EMOTION_REWRITE = AgentContextKeys::NODE_TYPE_EMOTION_REWRITE;
const QString &NODE_TYPE_OUTPUT_FORMAT = AgentContextKeys::NODE_TYPE_OUTPUT_FORMAT;
const QString &CONTEXT_KEY_CONVERSATION_HISTORY = AgentContextKeys::CONVERSATION_HISTORY;
const QString &CONTEXT_KEY_EMOTION_OUTPUT_TEXT = AgentContextKeys::EMOTION_OUTPUT_TEXT;
const QString &CONTEXT_KEY_INPUT_AVAILABLE = AgentContextKeys::INPUT_AVAILABLE;
const QString &CONTEXT_KEY_NODE_INPUT_PROMPT = AgentContextKeys::NODE_INPUT_PROMPT;
const QString &CONTEXT_KEY_NODE_INPUT_TEXT_RESPONSE = AgentContextKeys::NODE_INPUT_TEXT_RESPONSE;
const QString &CONTEXT_KEY_NODE_OUTPUT_TEXT_FINAL = AgentContextKeys::NODE_OUTPUT_TEXT_FINAL;
const QString &CONTEXT_KEY_NODE_OUTPUT_TEXT_RESPONSE = AgentContextKeys::NODE_OUTPUT_TEXT_RESPONSE;
const QString &CONTEXT_KEY_VISION_INPUT_READY = AgentContextKeys::VISION_INPUT_READY;
const QString &CONTEXT_KEY_PROMPT_TEXT = AgentContextKeys::PROMPT_TEXT;
const QString &CONTEXT_KEY_SEMANTIC_TEXT_FINAL = AgentContextKeys::SEMANTIC_TEXT_FINAL;
const QString &CONTEXT_KEY_SEMANTIC_TEXT_PROMPT = AgentContextKeys::SEMANTIC_TEXT_PROMPT;
const QString &CONTEXT_KEY_SEMANTIC_TEXT_RESPONSE = AgentContextKeys::SEMANTIC_TEXT_RESPONSE;
const QString &CONTEXT_KEY_SEMANTIC_VISION_SUMMARY = AgentContextKeys::SEMANTIC_VISION_SUMMARY;
const QString &CONTEXT_KEY_LLM_LAST_REQUEST_ID = AgentContextKeys::LLM_LAST_REQUEST_ID;
const QString &CONTEXT_KEY_LLM_PENDING = AgentContextKeys::LLM_PENDING;
const QString &CONTEXT_KEY_OUTPUT_PENDING = AgentContextKeys::OUTPUT_PENDING;
const QString &CONTEXT_KEY_OUTPUT_TEXT = AgentContextKeys::OUTPUT_TEXT;
const QString &CONTEXT_KEY_RUNTIME_PENDING = AgentContextKeys::RUNTIME_PENDING;
const QString &CONTEXT_KEY_RUNTIME_PENDING_NODE_ID = AgentContextKeys::RUNTIME_PENDING_NODE_ID;
const QString &CONTEXT_KEY_RUNTIME_PENDING_NODE_TYPE = AgentContextKeys::RUNTIME_PENDING_NODE_TYPE;
const QString &CONTEXT_KEY_RUNTIME_PENDING_REQUEST_ID = AgentContextKeys::RUNTIME_PENDING_REQUEST_ID;
const QString &CONTEXT_KEY_RUNTIME_PENDING_RESUME_INDEX = AgentContextKeys::RUNTIME_PENDING_RESUME_INDEX;
const QString &CONTEXT_KEY_RUNTIME_LAST_NODE_TYPE = AgentContextKeys::RUNTIME_LAST_NODE_TYPE;
const QString &CONTEXT_KEY_RUNTIME_PASS_THROUGH_PREFIX = AgentContextKeys::RUNTIME_PASS_THROUGH_PREFIX;
const QString &CONTEXT_KEY_VISION_ANALYSIS = AgentContextKeys::VISION_ANALYSIS;
const QString &CONTEXT_KEY_VISION_AVAILABLE = AgentContextKeys::VISION_AVAILABLE;
const QString &CONTEXT_KEY_VISION_LATEST_BASE64 = AgentContextKeys::VISION_LATEST_BASE64;
const QString &CONTEXT_KEY_VISION_LATEST_FRAME_ID = AgentContextKeys::VISION_LATEST_FRAME_ID;
const QString &CONTEXT_KEY_VISION_LATEST_WIDTH = AgentContextKeys::VISION_LATEST_WIDTH;
const QString &CONTEXT_KEY_VISION_LATEST_HEIGHT = AgentContextKeys::VISION_LATEST_HEIGHT;
const QString &CONTEXT_KEY_VISION_LATEST_MODALITY = AgentContextKeys::VISION_LATEST_MODALITY;
const QString &CONTEXT_KEY_VISION_LLM_LAST_REQUEST_ID = AgentContextKeys::VISION_LLM_LAST_REQUEST_ID;
const QString &CONTEXT_KEY_VISION_LLM_PENDING = AgentContextKeys::VISION_LLM_PENDING;
const QString &CONTEXT_KEY_VISION_UPDATED_AT = AgentContextKeys::VISION_UPDATED_AT;
constexpr int MAX_CONVERSATION_HISTORY_ITEMS = 60;

} // anonymous namespace

AgentRuntime::AgentRuntime(QObject *parent)
    : QObject(parent)
    , m_dagGraph()
    , m_context()
    , m_llmClient(new LlmClient(this))
    , m_visionLlmClient(new VisionLlmClient(this))
    , m_nodeHandlers()
    , m_executionOrder()
    , m_pendingResumeIndex(-1)
    , m_pendingNodeType()
    , m_pendingRequestId(-1)
    , m_isLoaded(false)
{
    connect(m_llmClient, &LlmClient::ChatCompleted,
            this, &AgentRuntime::OnLlmChatCompleted);
    connect(m_llmClient, &LlmClient::ChatFailed,
            this, &AgentRuntime::OnLlmChatFailed);
    connect(m_visionLlmClient, &VisionLlmClient::AnalysisCompleted,
            this, &AgentRuntime::OnVisionAnalysisCompleted);
    connect(m_visionLlmClient, &VisionLlmClient::AnalysisFailed,
            this, &AgentRuntime::OnVisionAnalysisFailed);

    RegisterDefaultNodeHandlers();
}

AgentRuntime::~AgentRuntime()
{
    // LLM 客户端由 QObject 父子关系自动销毁。
}

bool AgentRuntime::Load(const QString &configPath, QString &errorMessage)
{
    if (configPath.trimmed().isEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime config path is empty.");
        return false;
    }

    if (!m_dagGraph.LoadFromJsonFile(configPath, errorMessage))
    {
        m_isLoaded = false;
        m_executionOrder.clear();
        return false;
    }

    if (!m_dagGraph.TopologicalSort(m_executionOrder, errorMessage))
    {
        m_isLoaded = false;
        m_executionOrder.clear();
        return false;
    }

    m_isLoaded = true;

    qDebug() << "[Agent] DAG loaded:" << configPath;
    qDebug() << "[Agent] Topological order:" << m_executionOrder;

    return true;
}

bool AgentRuntime::Execute(QString &errorMessage)
{
    if (!m_isLoaded)
    {
        errorMessage = QStringLiteral("Agent runtime is not loaded.");
        return false;
    }

    if (m_executionOrder.isEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime execution order is empty.");
        return false;
    }

    return ExecuteFromIndex(0, true, errorMessage);
}

bool AgentRuntime::ExecuteWithUserInput(const QString &userInput, QString &errorMessage)
{
    const QString normalizedUserInput = userInput.trimmed();

    if (normalizedUserInput.isEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime user input is empty.");
        return false;
    }

    if (!m_context.SetUserInput(normalizedUserInput))
    {
        errorMessage = QStringLiteral("Agent runtime failed to set user input.");
        return false;
    }

    if (m_isLoaded)
    {
        if (!Execute(errorMessage))
        {
            return false;
        }

        return true;
    }

    emit LogMessage(QStringLiteral("Agent DAG is not loaded. Voice text will use direct LLM fallback."));

    if (!PrepareTextInputContext(m_context, errorMessage))
    {
        return false;
    }

    return SendUserInputToLlm(normalizedUserInput, errorMessage);
}

bool AgentRuntime::LoadLlmConfig(const QString &configPath, QString &errorMessage)
{
    if (configPath.trimmed().isEmpty())
    {
        errorMessage = QStringLiteral("Agent LLM config path is empty.");
        return false;
    }

    if (m_llmClient == nullptr)
    {
        errorMessage = QStringLiteral("Agent LLM client is not initialized.");
        return false;
    }

    if (!m_llmClient->LoadConfig(configPath))
    {
        errorMessage = QStringLiteral("Agent failed to load LLM config.");
        return false;
    }

    emit LogMessage(QStringLiteral("Agent LLM config loaded: %1").arg(configPath));

    return true;
}

bool AgentRuntime::LoadVisionLlmConfig(const QString &configPath, QString &errorMessage)
{
    if (configPath.trimmed().isEmpty())
    {
        errorMessage = QStringLiteral("Agent Vision LLM config path is empty.");
        return false;
    }

    if (m_visionLlmClient == nullptr)
    {
        errorMessage = QStringLiteral("Agent Vision LLM client is not initialized.");
        return false;
    }

    if (!m_visionLlmClient->LoadConfig(configPath))
    {
        errorMessage = QStringLiteral("Agent failed to load Vision LLM config.");
        return false;
    }

    emit LogMessage(QStringLiteral("Agent Vision LLM config loaded: %1").arg(configPath));

    return true;
}

bool AgentRuntime::UpdatePerceptionFrame(const QByteArray &encodedData,
                                         int frameId,
                                         const QSize &frameSize,
                                         const QString &modality,
                                         QString &errorMessage)
{
    const QString normalizedModality = modality.trimmed();

    if (encodedData.isEmpty())
    {
        errorMessage = QStringLiteral("Agent perception frame data is empty.");
        return false;
    }

    if (frameId <= 0)
    {
        errorMessage = QStringLiteral("Agent perception frame ID is invalid.");
        return false;
    }

    if (!frameSize.isValid())
    {
        errorMessage = QStringLiteral("Agent perception frame size is invalid.");
        return false;
    }

    if (normalizedModality.isEmpty())
    {
        errorMessage = QStringLiteral("Agent perception modality is empty.");
        return false;
    }

    if (!m_context.SetValue(CONTEXT_KEY_VISION_AVAILABLE, true))
    {
        errorMessage = QStringLiteral("Agent failed to record vision availability.");
        return false;
    }

    m_context.RemoveValue(CONTEXT_KEY_VISION_ANALYSIS);
    m_context.RemoveValue(CONTEXT_KEY_SEMANTIC_VISION_SUMMARY);
    m_context.RemoveValue(CONTEXT_KEY_VISION_LLM_LAST_REQUEST_ID);
    m_context.RemoveValue(CONTEXT_KEY_VISION_LLM_PENDING);

    if (!m_context.SetValue(CONTEXT_KEY_VISION_LATEST_BASE64, QString::fromLatin1(encodedData)))
    {
        errorMessage = QStringLiteral("Agent failed to record latest vision frame.");
        return false;
    }

    if (!m_context.SetValue(CONTEXT_KEY_VISION_LATEST_FRAME_ID, frameId))
    {
        errorMessage = QStringLiteral("Agent failed to record latest vision frame ID.");
        return false;
    }

    if (!m_context.SetValue(CONTEXT_KEY_VISION_LATEST_WIDTH, frameSize.width()))
    {
        errorMessage = QStringLiteral("Agent failed to record latest vision frame width.");
        return false;
    }

    if (!m_context.SetValue(CONTEXT_KEY_VISION_LATEST_HEIGHT, frameSize.height()))
    {
        errorMessage = QStringLiteral("Agent failed to record latest vision frame height.");
        return false;
    }

    if (!m_context.SetValue(CONTEXT_KEY_VISION_LATEST_MODALITY, normalizedModality))
    {
        errorMessage = QStringLiteral("Agent failed to record latest vision modality.");
        return false;
    }

    if (!m_context.SetValue(CONTEXT_KEY_VISION_UPDATED_AT,
                            QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)))
    {
        errorMessage = QStringLiteral("Agent failed to record latest vision timestamp.");
        return false;
    }

    emit LogMessage(QStringLiteral("Agent perception frame updated: %1").arg(frameId));

    return true;
}

bool AgentRuntime::LoadDefaultLlmConfig(QString &errorMessage)
{
    const QString configPath = FindDefaultLlmConfigPath();

    if (configPath.isEmpty())
    {
        errorMessage = QStringLiteral("llm_config.json not found.");
        return false;
    }

    return LoadLlmConfig(configPath, errorMessage);
}

bool AgentRuntime::LoadDefaultVisionLlmConfig(QString &errorMessage)
{
    const QString configPath = FindDefaultVisionLlmConfigPath();

    if (configPath.isEmpty())
    {
        errorMessage = QStringLiteral("vision_llm_config.json not found.");
        return false;
    }

    return LoadVisionLlmConfig(configPath, errorMessage);
}

bool AgentRuntime::IsLlmConfigured() const
{
    if (m_llmClient == nullptr)
    {
        return false;
    }

    return m_llmClient->IsConfigured();
}

bool AgentRuntime::IsVisionLlmConfigured() const
{
    if (m_visionLlmClient == nullptr)
    {
        return false;
    }

    return m_visionLlmClient->IsConfigured();
}

bool AgentRuntime::HasPendingAsyncRequest() const
{
    QVariant pendingValue;

    if (!m_context.GetValue(CONTEXT_KEY_RUNTIME_PENDING, pendingValue))
    {
        return false;
    }

    return pendingValue.toBool();
}

bool AgentRuntime::Start(const QString &configPath, QString &errorMessage)
{
    if (!Load(configPath, errorMessage))
    {
        return false;
    }

    return Execute(errorMessage);
}

QVector<QString> AgentRuntime::GetExecutionOrder() const
{
    return m_executionOrder;
}

AgentContext &AgentRuntime::GetContext()
{
    return m_context;
}

const AgentContext &AgentRuntime::GetContext() const
{
    return m_context;
}

void AgentRuntime::SetContext(const AgentContext &context)
{
    m_context = context;
}

bool AgentRuntime::RegisterNodeHandler(const QString &nodeType, const NodeHandler &handler)
{
    const QString normalizedNodeType = nodeType.trimmed();

    if (normalizedNodeType.isEmpty())
    {
        return false;
    }

    if (!handler)
    {
        return false;
    }

    m_nodeHandlers.insert(normalizedNodeType, handler);

    return true;
}

bool AgentRuntime::ExecuteNode(const _tagAgentDagNode &node,
                               AgentContext &context,
                               QString &errorMessage)
{
    const QString nodeId = node.id.trimmed();
    const QString nodeType = node.type.trimmed();

    if (nodeId.isEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime node id is empty.");
        return false;
    }

    if (nodeType.isEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime node type is empty: %1").arg(nodeId);
        return false;
    }

    if (!context.AppendExecutedNode(nodeId))
    {
        errorMessage = QStringLiteral("Agent runtime failed to record executed node.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_RUNTIME_LAST_NODE_TYPE, nodeType))
    {
        errorMessage = QStringLiteral("Agent runtime failed to record node type: %1").arg(nodeId);
        return false;
    }

    if (!m_nodeHandlers.contains(nodeType))
    {
        errorMessage = QStringLiteral("Agent node handler is not registered: %1").arg(nodeType);
        return false;
    }

    const NodeHandler handler = m_nodeHandlers.value(nodeType);

    if (!handler)
    {
        errorMessage = QStringLiteral("Agent node handler is invalid: %1").arg(nodeType);
        return false;
    }

    if (!PrepareNodeInputAliases(node, context))
    {
        errorMessage = QStringLiteral("Agent runtime failed to prepare node input aliases: %1").arg(
                           nodeId);
        return false;
    }

    if (!handler(node, context, errorMessage))
    {
        return false;
    }

    if (!SyncNodeOutputAliases(node, context))
    {
        errorMessage = QStringLiteral("Agent runtime failed to sync node output aliases: %1").arg(
                           nodeId);
        return false;
    }

    return true;
}

bool AgentRuntime::PrepareNodeInputAliases(const _tagAgentDagNode &node, AgentContext &context)
{
    const QString nodeType = node.type.trimmed();

    if (nodeType.isEmpty())
    {
        return false;
    }

    QVariant value;

    if (nodeType == NODE_TYPE_LLM_CHAT)
    {
        if (context.GetValue(CONTEXT_KEY_SEMANTIC_TEXT_PROMPT, value))
        {
            return context.SetValue(CONTEXT_KEY_PROMPT_TEXT, value);
        }

        if (context.GetValue(CONTEXT_KEY_PROMPT_TEXT, value))
        {
            return context.SetValue(CONTEXT_KEY_NODE_INPUT_PROMPT, value);
        }

        return true;
    }

    if (nodeType == NODE_TYPE_EMOTION_REWRITE)
    {
        if (context.GetValue(CONTEXT_KEY_SEMANTIC_TEXT_RESPONSE, value))
        {
            if (!context.SetValue(AgentContextKeys::LLM_LAST_RESPONSE, value))
            {
                return false;
            }

            return context.SetValue(CONTEXT_KEY_NODE_INPUT_TEXT_RESPONSE, value);
        }

        if (context.GetValue(AgentContextKeys::LLM_LAST_RESPONSE, value))
        {
            return context.SetValue(CONTEXT_KEY_NODE_INPUT_TEXT_RESPONSE, value);
        }

        return true;
    }

    if (nodeType == NODE_TYPE_OUTPUT_FORMAT)
    {
        if (context.GetValue(CONTEXT_KEY_SEMANTIC_TEXT_RESPONSE, value))
        {
            return context.SetValue(CONTEXT_KEY_NODE_INPUT_TEXT_RESPONSE, value);
        }

        return true;
    }

    return true;
}

bool AgentRuntime::SyncNodeOutputAliases(const _tagAgentDagNode &node, AgentContext &context)
{
    const QString nodeType = node.type.trimmed();

    if (nodeType.isEmpty())
    {
        return false;
    }

    QVariant value;

    if (nodeType == NODE_TYPE_LLM_CHAT)
    {
        if (context.GetValue(AgentContextKeys::LLM_LAST_RESPONSE, value))
        {
            return context.SetValue(CONTEXT_KEY_SEMANTIC_TEXT_RESPONSE, value)
                   && context.SetValue(CONTEXT_KEY_NODE_OUTPUT_TEXT_RESPONSE, value);
        }

        return true;
    }

    if (nodeType == NODE_TYPE_EMOTION_REWRITE)
    {
        if (context.GetValue(CONTEXT_KEY_EMOTION_OUTPUT_TEXT, value))
        {
            return context.SetValue(CONTEXT_KEY_SEMANTIC_TEXT_RESPONSE, value)
                   && context.SetValue(CONTEXT_KEY_NODE_OUTPUT_TEXT_RESPONSE, value);
        }

        return true;
    }

    if (nodeType == NODE_TYPE_OUTPUT_FORMAT)
    {
        if (context.GetValue(CONTEXT_KEY_OUTPUT_TEXT, value))
        {
            return context.SetValue(CONTEXT_KEY_SEMANTIC_TEXT_FINAL, value)
                   && context.SetValue(CONTEXT_KEY_NODE_OUTPUT_TEXT_FINAL, value);
        }

        return true;
    }

    return true;
}

bool AgentRuntime::ExecuteFromIndex(int startIndex,
                                    bool shouldPrepareInput,
                                    QString &errorMessage)
{
    if (startIndex < 0)
    {
        errorMessage = QStringLiteral("Agent runtime start index is invalid.");
        return false;
    }

    if (startIndex > m_executionOrder.size())
    {
        errorMessage = QStringLiteral("Agent runtime start index is out of range.");
        return false;
    }

    if (shouldPrepareInput && !PrepareTextInputContext(m_context, errorMessage))
    {
        return false;
    }

    qDebug() << "[Agent] Execute from index:" << startIndex;

    for (int nodeIndex = startIndex; nodeIndex < m_executionOrder.size(); ++nodeIndex)
    {
        const QString nodeName = m_executionOrder.at(nodeIndex);
        _tagAgentDagNode node;

        if (!m_dagGraph.GetNode(nodeName, node))
        {
            errorMessage = QStringLiteral("Agent runtime node definition is missing: %1").arg(
                               nodeName);
            return false;
        }

        if (!ExecuteNode(node, m_context, errorMessage))
        {
            return false;
        }

        QVariant pendingValue;

        if (m_context.GetValue(CONTEXT_KEY_RUNTIME_PENDING, pendingValue)
            && pendingValue.toBool())
        {
            m_pendingResumeIndex = nodeIndex + 1;
            m_pendingNodeType = node.type.trimmed();

            QVariant requestIdValue;

            if (m_context.GetValue(CONTEXT_KEY_RUNTIME_PENDING_REQUEST_ID, requestIdValue))
            {
                m_pendingRequestId = requestIdValue.toInt();
            }
            else
            {
                m_pendingRequestId = -1;
            }

            m_context.SetValue(CONTEXT_KEY_RUNTIME_PENDING_RESUME_INDEX, m_pendingResumeIndex);
            qDebug() << "[Agent] Execute paused for LLM response at index:" << m_pendingResumeIndex;
            return true;
        }
    }

    m_pendingResumeIndex = -1;
    m_pendingNodeType.clear();
    m_pendingRequestId = -1;
    qDebug() << "[Agent] Execute finished.";

    return true;
}

bool AgentRuntime::PrepareTextInputContext(AgentContext &context, QString &errorMessage)
{
    const QString userInput = context.GetUserInput().trimmed();
    const bool isInputAvailable = !userInput.isEmpty();

    if (!context.SetValue(CONTEXT_KEY_INPUT_AVAILABLE, isInputAvailable))
    {
        errorMessage = QStringLiteral("Agent failed to record input state.");
        return false;
    }

    context.RemoveValue(AgentContextKeys::LLM_LAST_RESPONSE);
    context.RemoveValue(CONTEXT_KEY_EMOTION_OUTPUT_TEXT);
    context.RemoveValue(CONTEXT_KEY_LLM_LAST_REQUEST_ID);
    context.RemoveValue(CONTEXT_KEY_NODE_INPUT_PROMPT);
    context.RemoveValue(CONTEXT_KEY_NODE_INPUT_TEXT_RESPONSE);
    context.RemoveValue(CONTEXT_KEY_NODE_OUTPUT_TEXT_FINAL);
    context.RemoveValue(CONTEXT_KEY_NODE_OUTPUT_TEXT_RESPONSE);
    context.RemoveValue(CONTEXT_KEY_OUTPUT_TEXT);
    context.RemoveValue(CONTEXT_KEY_OUTPUT_PENDING);
    context.RemoveValue(CONTEXT_KEY_SEMANTIC_TEXT_FINAL);
    context.RemoveValue(CONTEXT_KEY_SEMANTIC_TEXT_RESPONSE);
    ClearAsyncPendingState(context);

    if (!isInputAvailable)
    {
        context.RemoveValue(CONTEXT_KEY_PROMPT_TEXT);
        emit LogMessage(QStringLiteral("Agent text input is empty."));
        return true;
    }

    if (!context.SetValue(CONTEXT_KEY_PROMPT_TEXT, userInput))
    {
        errorMessage = QStringLiteral("Agent failed to write prompt text.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_SEMANTIC_TEXT_PROMPT, userInput))
    {
        errorMessage = QStringLiteral("Agent failed to write semantic prompt text.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_NODE_INPUT_PROMPT, userInput))
    {
        errorMessage = QStringLiteral("Agent failed to write node prompt input.");
        return false;
    }

    emit LogMessage(QStringLiteral("Agent text input prepared."));

    return true;
}

bool AgentRuntime::ExecuteVisionInputNode(const _tagAgentDagNode &node,
                                          AgentContext &context,
                                          QString &errorMessage)
{
    if (node.id.trimmed().isEmpty())
    {
        errorMessage = QStringLiteral("Agent vision input node id is empty.");
        return false;
    }

    QVariant visionBase64Value;
    QVariant visionFrameIdValue;
    QVariant visionWidthValue;
    QVariant visionHeightValue;
    QVariant visionModalityValue;

    const bool hasVisionBase64 = context.GetValue(CONTEXT_KEY_VISION_LATEST_BASE64,
                                                  visionBase64Value);
    const bool hasVisionFrameId = context.GetValue(CONTEXT_KEY_VISION_LATEST_FRAME_ID,
                                                  visionFrameIdValue);
    const bool hasVisionWidth = context.GetValue(CONTEXT_KEY_VISION_LATEST_WIDTH,
                                                visionWidthValue);
    const bool hasVisionHeight = context.GetValue(CONTEXT_KEY_VISION_LATEST_HEIGHT,
                                                 visionHeightValue);
    const bool hasVisionModality = context.GetValue(CONTEXT_KEY_VISION_LATEST_MODALITY,
                                                    visionModalityValue);

    const bool hasInput = hasVisionBase64
                          && hasVisionFrameId
                          && hasVisionWidth
                          && hasVisionHeight
                          && hasVisionModality
                          && !visionBase64Value.toString().trimmed().isEmpty()
                          && (visionFrameIdValue.toInt() > 0)
                          && (visionWidthValue.toInt() > 0)
                          && (visionHeightValue.toInt() > 0)
                          && !visionModalityValue.toString().trimmed().isEmpty();

    if (!context.SetValue(CONTEXT_KEY_VISION_INPUT_READY, hasInput))
    {
        errorMessage = QStringLiteral("Agent vision input node failed to record readiness.");
        return false;
    }

    if (hasInput)
    {
        emit LogMessage(QStringLiteral("Agent vision input is ready."));
    }
    else
    {
        emit LogMessage(QStringLiteral("Agent vision input node is waiting for perception data."));
    }

    return true;
}

bool AgentRuntime::ExecuteVisionLlmNode(const _tagAgentDagNode &node,
                                        AgentContext &context,
                                        QString &errorMessage)
{
    if (node.id.trimmed().isEmpty())
    {
        errorMessage = QStringLiteral("Agent Vision LLM node id is empty.");
        return false;
    }

    QVariant readyValue;
    QVariant inputAvailableValue;

    if (context.GetValue(CONTEXT_KEY_INPUT_AVAILABLE, inputAvailableValue)
        && inputAvailableValue.toBool())
    {
        emit LogMessage(QStringLiteral("Agent Vision LLM node skipped during text input execution."));
        return true;
    }

    if (!context.GetValue(CONTEXT_KEY_VISION_INPUT_READY, readyValue)
        || !readyValue.toBool())
    {
        emit LogMessage(QStringLiteral("Agent Vision LLM node skipped because vision input is not ready."));
        return true;
    }

    QVariant imageValue;
    QVariant modalityValue;

    if (!context.GetValue(CONTEXT_KEY_VISION_LATEST_BASE64, imageValue))
    {
        errorMessage = QStringLiteral("Agent Vision LLM node image data is missing.");
        return false;
    }

    if (!context.GetValue(CONTEXT_KEY_VISION_LATEST_MODALITY, modalityValue))
    {
        errorMessage = QStringLiteral("Agent Vision LLM node modality is missing.");
        return false;
    }

    const QByteArray base64Image = imageValue.toString().trimmed().toLatin1();
    QString mediaType = QStringLiteral("image/png");

    if (modalityValue.toString().contains(QStringLiteral("jpeg"), Qt::CaseInsensitive))
    {
        mediaType = QStringLiteral("image/jpeg");
    }

    if (base64Image.isEmpty())
    {
        errorMessage = QStringLiteral("Agent Vision LLM node image data is empty.");
        return false;
    }

    if ((m_visionLlmClient == nullptr) || !m_visionLlmClient->IsConfigured())
    {
        errorMessage = QStringLiteral("Agent Vision LLM client is not configured.");
        return false;
    }

    const QString prompt = node.config.value(QStringLiteral("prompt")).toString().trimmed();
    const QString requestPrompt = prompt.isEmpty()
                                  ? QStringLiteral("请用简洁中文描述当前屏幕画面，重点说明可用于主动开启话题的内容。")
                                  : prompt;
    const int requestId = m_visionLlmClient->AnalyzeScreenshot(requestPrompt,
                                                               base64Image,
                                                               mediaType);

    if (requestId <= 0)
    {
        errorMessage = QStringLiteral("Agent Vision LLM node failed to send request.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_VISION_LLM_LAST_REQUEST_ID, requestId))
    {
        errorMessage = QStringLiteral("Agent Vision LLM node failed to record request ID.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_VISION_LLM_PENDING, true))
    {
        errorMessage = QStringLiteral("Agent Vision LLM node failed to record pending state.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_RUNTIME_PENDING, true))
    {
        errorMessage = QStringLiteral("Agent Vision LLM node failed to record runtime pending state.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_RUNTIME_PENDING_NODE_ID, node.id.trimmed()))
    {
        errorMessage = QStringLiteral("Agent Vision LLM node failed to record runtime pending node id.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_RUNTIME_PENDING_NODE_TYPE, node.type.trimmed()))
    {
        errorMessage = QStringLiteral("Agent Vision LLM node failed to record runtime pending node type.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_RUNTIME_PENDING_REQUEST_ID, requestId))
    {
        errorMessage = QStringLiteral("Agent Vision LLM node failed to record runtime pending request ID.");
        return false;
    }

    emit LogMessage(QStringLiteral("Agent Vision LLM node request sent: %1").arg(requestId));

    return true;
}

bool AgentRuntime::ExecuteLlmChatNode(const _tagAgentDagNode &node,
                                      AgentContext &context,
                                      QString &errorMessage)
{
    if (node.id.trimmed().isEmpty())
    {
        errorMessage = QStringLiteral("Agent LLM node id is empty.");
        return false;
    }

    QVariant promptValue;

    if (!context.GetValue(CONTEXT_KEY_PROMPT_TEXT, promptValue))
    {
        emit LogMessage(QStringLiteral("Agent LLM node skipped because prompt is empty."));
        return true;
    }

    const QString promptText = promptValue.toString().trimmed();

    if (promptText.isEmpty())
    {
        emit LogMessage(QStringLiteral("Agent LLM node skipped because prompt text is empty."));
        return true;
    }

    if ((m_llmClient == nullptr) || !m_llmClient->IsConfigured())
    {
        errorMessage = QStringLiteral("Agent LLM client is not configured.");
        return false;
    }

    const int requestId = m_llmClient->SendPrompt(promptText);

    if (requestId <= 0)
    {
        errorMessage = QStringLiteral("Agent LLM node failed to send request.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_LLM_LAST_REQUEST_ID, requestId))
    {
        errorMessage = QStringLiteral("Agent LLM node failed to record request ID.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_LLM_PENDING, true))
    {
        errorMessage = QStringLiteral("Agent LLM node failed to record pending state.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_RUNTIME_PENDING, true))
    {
        errorMessage = QStringLiteral("Agent LLM node failed to record runtime pending state.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_RUNTIME_PENDING_NODE_ID, node.id.trimmed()))
    {
        errorMessage = QStringLiteral("Agent LLM node failed to record runtime pending node id.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_RUNTIME_PENDING_NODE_TYPE, node.type.trimmed()))
    {
        errorMessage = QStringLiteral("Agent LLM node failed to record runtime pending node type.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_RUNTIME_PENDING_REQUEST_ID, requestId))
    {
        errorMessage = QStringLiteral("Agent LLM node failed to record runtime pending request ID.");
        return false;
    }

    emit LogMessage(QStringLiteral("Agent LLM node request sent: %1").arg(requestId));

    return true;
}

bool AgentRuntime::ExecuteOutputFormatNode(const _tagAgentDagNode &node,
                                           AgentContext &context,
                                           QString &errorMessage)
{
    if (node.id.trimmed().isEmpty())
    {
        errorMessage = QStringLiteral("Agent output node id is empty.");
        return false;
    }

    QVariant outputValue;

    if (!context.GetValue(CONTEXT_KEY_SEMANTIC_TEXT_FINAL, outputValue)
        && !context.GetValue(CONTEXT_KEY_EMOTION_OUTPUT_TEXT, outputValue)
        && !context.GetValue(CONTEXT_KEY_SEMANTIC_TEXT_RESPONSE, outputValue)
        && !context.GetValue(CONTEXT_KEY_NODE_INPUT_TEXT_RESPONSE, outputValue)
        && !context.GetValue(AgentContextKeys::LLM_LAST_RESPONSE, outputValue))
    {
        if (!context.SetValue(CONTEXT_KEY_OUTPUT_PENDING, true))
        {
            errorMessage = QStringLiteral("Agent output node failed to record pending state.");
            return false;
        }

        emit LogMessage(QStringLiteral("Agent output node is waiting for LLM response."));
        return true;
    }

    const QString responseText = outputValue.toString().trimmed();

    if (responseText.isEmpty())
    {
        errorMessage = QStringLiteral("Agent output node received empty LLM response.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_OUTPUT_TEXT, responseText))
    {
        errorMessage = QStringLiteral("Agent output node failed to write output text.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_SEMANTIC_TEXT_FINAL, responseText))
    {
        errorMessage = QStringLiteral("Agent output node failed to write semantic final text.");
        return false;
    }

    if (!AppendConversationHistory(context, responseText, errorMessage))
    {
        return false;
    }

    context.RemoveValue(CONTEXT_KEY_OUTPUT_PENDING);
    emit LogMessage(QStringLiteral("Agent output formatted."));

    return true;
}

bool AgentRuntime::AppendConversationHistory(AgentContext &context,
                                             const QString &outputText,
                                             QString &errorMessage)
{
    const QString normalizedUserInput = context.GetUserInput().trimmed();
    const QString normalizedOutputText = outputText.trimmed();

    if (normalizedUserInput.isEmpty() || normalizedOutputText.isEmpty())
    {
        errorMessage = QStringLiteral("Agent conversation history input is empty.");
        return false;
    }

    QVariant historyValue;
    QStringList conversationHistory;

    if (context.GetValue(CONTEXT_KEY_CONVERSATION_HISTORY, historyValue))
    {
        conversationHistory = historyValue.toStringList();
    }

    conversationHistory.append(QStringLiteral("user: %1").arg(normalizedUserInput));
    conversationHistory.append(QStringLiteral("assistant: %1").arg(normalizedOutputText));

    while (conversationHistory.size() > MAX_CONVERSATION_HISTORY_ITEMS)
    {
        conversationHistory.removeFirst();
    }

    if (!context.SetValue(CONTEXT_KEY_CONVERSATION_HISTORY, conversationHistory))
    {
        errorMessage = QStringLiteral("Agent failed to record conversation history.");
        return false;
    }

    return true;
}

void AgentRuntime::ClearAsyncPendingState(AgentContext &context)
{
    context.RemoveValue(CONTEXT_KEY_RUNTIME_PENDING);
    context.RemoveValue(CONTEXT_KEY_RUNTIME_PENDING_NODE_ID);
    context.RemoveValue(CONTEXT_KEY_RUNTIME_PENDING_NODE_TYPE);
    context.RemoveValue(CONTEXT_KEY_RUNTIME_PENDING_REQUEST_ID);
    context.RemoveValue(CONTEXT_KEY_RUNTIME_PENDING_RESUME_INDEX);
}

bool AgentRuntime::ExecutePassThroughNode(const _tagAgentDagNode &node,
                                          AgentContext &context,
                                          QString &errorMessage)
{
    const QString nodeId = node.id.trimmed();
    const QString nodeType = node.type.trimmed();

    if (nodeId.isEmpty() || nodeType.isEmpty())
    {
        errorMessage = QStringLiteral("Agent pass-through node is invalid.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_RUNTIME_PASS_THROUGH_PREFIX + nodeId, nodeType))
    {
        errorMessage = QStringLiteral("Agent pass-through node failed to write context: %1").arg(
                           nodeId);
        return false;
    }

    emit LogMessage(QStringLiteral("Agent pass-through node executed: %1 (%2)").arg(
                        nodeId,
                        nodeType));

    return true;
}

void AgentRuntime::RegisterDefaultNodeHandlers()
{
    RegisterNodeHandler(NODE_TYPE_VISION_INPUT,
                        [this](const _tagAgentDagNode &node,
                               AgentContext &context,
                               QString &errorMessage)
    {
        return ExecuteVisionInputNode(node, context, errorMessage);
    });

    RegisterNodeHandler(NODE_TYPE_VISION_LLM,
                        [this](const _tagAgentDagNode &node,
                               AgentContext &context,
                               QString &errorMessage)
    {
        return ExecuteVisionLlmNode(node, context, errorMessage);
    });

    RegisterNodeHandler(NODE_TYPE_LLM_CHAT,
                        [this](const _tagAgentDagNode &node,
                               AgentContext &context,
                               QString &errorMessage)
    {
        return ExecuteLlmChatNode(node, context, errorMessage);
    });

    RegisterNodeHandler(NODE_TYPE_EMOTION_REWRITE,
                        [this](const _tagAgentDagNode &node,
                               AgentContext &context,
                               QString &errorMessage)
    {
        return EmotionRewriteNode::Execute(node, context, m_llmClient, errorMessage);
    });

    RegisterNodeHandler(NODE_TYPE_OUTPUT_FORMAT,
                        [this](const _tagAgentDagNode &node,
                               AgentContext &context,
                               QString &errorMessage)
    {
        return ExecuteOutputFormatNode(node, context, errorMessage);
    });
}

QString AgentRuntime::FindDefaultLlmConfigPath() const
{
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QStringList candidatePaths =
    {
        exeDir + QStringLiteral("/") + LLM_CONFIG_FILE_NAME,
        QDir::currentPath() + QStringLiteral("/") + LLM_CONFIG_FILE_NAME,
        exeDir + QStringLiteral("/../") + LLM_CONFIG_FILE_NAME,
        exeDir + QStringLiteral("/../../") + LLM_CONFIG_FILE_NAME
    };

    for (const QString &candidatePath : candidatePaths)
    {
        if (QFileInfo::exists(candidatePath))
        {
            return QFileInfo(candidatePath).absoluteFilePath();
        }
    }

    return QString();
}

QString AgentRuntime::FindDefaultVisionLlmConfigPath() const
{
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QStringList candidatePaths =
    {
        exeDir + QStringLiteral("/") + VISION_LLM_CONFIG_FILE_NAME,
        QDir::currentPath() + QStringLiteral("/") + VISION_LLM_CONFIG_FILE_NAME,
        exeDir + QStringLiteral("/../") + VISION_LLM_CONFIG_FILE_NAME,
        exeDir + QStringLiteral("/../../") + VISION_LLM_CONFIG_FILE_NAME
    };

    for (const QString &candidatePath : candidatePaths)
    {
        if (QFileInfo::exists(candidatePath))
        {
            return QFileInfo(candidatePath).absoluteFilePath();
        }
    }

    return QString();
}

bool AgentRuntime::SendUserInputToLlm(const QString &userInput, QString &errorMessage)
{
    const QString normalizedUserInput = userInput.trimmed();

    if (normalizedUserInput.isEmpty())
    {
        errorMessage = QStringLiteral("Agent LLM user input is empty.");
        return false;
    }

    if ((m_llmClient == nullptr) || !m_llmClient->IsConfigured())
    {
        errorMessage = QStringLiteral("Agent LLM client is not configured.");
        return false;
    }

    const int requestId = m_llmClient->SendPrompt(normalizedUserInput);

    if (requestId <= 0)
    {
        errorMessage = QStringLiteral("Agent failed to send LLM request.");
        return false;
    }

    emit LogMessage(QStringLiteral("Agent LLM request sent: %1").arg(requestId));

    return true;
}

void AgentRuntime::OnLlmChatCompleted(int requestId, const QString &content)
{
    if (requestId <= 0)
    {
        emit LlmRequestFailed(requestId, QStringLiteral("Agent LLM request ID is invalid."), 0);
        return;
    }

    if (content.isEmpty())
    {
        emit LlmRequestFailed(requestId, QStringLiteral("Agent LLM response is empty."), 0);
        return;
    }

    QVariant runtimePendingValue;
    QVariant pendingNodeTypeValue;
    QVariant pendingRequestIdValue;

    const bool hasRuntimePending = m_context.GetValue(CONTEXT_KEY_RUNTIME_PENDING,
                                                      runtimePendingValue)
                                   && runtimePendingValue.toBool()
                                   && m_context.GetValue(CONTEXT_KEY_RUNTIME_PENDING_NODE_TYPE,
                                                         pendingNodeTypeValue)
                                   && m_context.GetValue(CONTEXT_KEY_RUNTIME_PENDING_REQUEST_ID,
                                                         pendingRequestIdValue);

    if (hasRuntimePending && (pendingRequestIdValue.toInt() != requestId))
    {
        emit LlmRequestFailed(requestId, QStringLiteral("Agent LLM request ID does not match pending node."), 0);
        return;
    }

    const QString pendingNodeType = hasRuntimePending
                                    ? pendingNodeTypeValue.toString().trimmed()
                                    : QString();

    if (pendingNodeType == NODE_TYPE_EMOTION_REWRITE)
    {
        QString errorMessage;

        if (!EmotionRewriteNode::Complete(requestId, content, m_context, errorMessage))
        {
            m_pendingResumeIndex = -1;
            m_pendingNodeType.clear();
            m_pendingRequestId = -1;
            emit LlmRequestFailed(requestId, errorMessage, 0);
            return;
        }

        if (m_pendingResumeIndex >= 0)
        {
            if (!ExecuteFromIndex(m_pendingResumeIndex, false, errorMessage))
            {
                m_pendingResumeIndex = -1;
                m_pendingNodeType.clear();
                m_pendingRequestId = -1;
                emit LlmRequestFailed(requestId, errorMessage, 0);
                return;
            }
        }

        QVariant outputValue;

        if (m_context.GetValue(CONTEXT_KEY_OUTPUT_TEXT, outputValue))
        {
            emit LlmResponseReceived(requestId, outputValue.toString());
            return;
        }

        return;
    }

    if (!m_context.SetValue(AgentContextKeys::LLM_LAST_RESPONSE, content))
    {
        emit LlmRequestFailed(requestId, QStringLiteral("Agent failed to record LLM response."), 0);
        return;
    }

    if (!m_context.SetValue(CONTEXT_KEY_LLM_PENDING, false))
    {
        emit LlmRequestFailed(requestId, QStringLiteral("Agent failed to clear LLM pending state."), 0);
        return;
    }

    ClearAsyncPendingState(m_context);

    if (m_pendingResumeIndex >= 0)
    {
        QString errorMessage;

        if (!ExecuteFromIndex(m_pendingResumeIndex, false, errorMessage))
        {
            m_pendingResumeIndex = -1;
            m_pendingNodeType.clear();
            m_pendingRequestId = -1;
            emit LlmRequestFailed(requestId, errorMessage, 0);
            return;
        }

        QVariant runtimePendingAfterResume;

        if (m_context.GetValue(CONTEXT_KEY_RUNTIME_PENDING, runtimePendingAfterResume)
            && runtimePendingAfterResume.toBool())
        {
            return;
        }

        QVariant outputValue;

        if (m_context.GetValue(CONTEXT_KEY_OUTPUT_TEXT, outputValue))
        {
            emit LlmResponseReceived(requestId, outputValue.toString());
            return;
        }
    }

    if (!m_context.SetValue(CONTEXT_KEY_OUTPUT_TEXT, content))
    {
        emit LlmRequestFailed(requestId, QStringLiteral("Agent failed to record output text."), 0);
        return;
    }

    m_context.RemoveValue(CONTEXT_KEY_OUTPUT_PENDING);
    emit LlmResponseReceived(requestId, content);
}

void AgentRuntime::OnLlmChatFailed(int requestId, const QString &message, int statusCode)
{
    emit LlmRequestFailed(requestId, message, statusCode);
}

void AgentRuntime::OnVisionAnalysisCompleted(int requestId, const QString &content)
{
    if (requestId <= 0)
    {
        emit LogMessage(QStringLiteral("Agent Vision LLM request ID is invalid."));
        return;
    }

    const QString normalizedContent = content.trimmed();

    if (normalizedContent.isEmpty())
    {
        emit LogMessage(QStringLiteral("Agent Vision LLM response is empty."));
        return;
    }

    QVariant pendingNodeTypeValue;
    QVariant pendingRequestIdValue;

    if (!m_context.GetValue(CONTEXT_KEY_RUNTIME_PENDING_NODE_TYPE, pendingNodeTypeValue)
        || !m_context.GetValue(CONTEXT_KEY_RUNTIME_PENDING_REQUEST_ID, pendingRequestIdValue))
    {
        emit LogMessage(QStringLiteral("Agent Vision LLM response ignored because no pending node exists."));
        return;
    }

    if ((pendingNodeTypeValue.toString().trimmed() != NODE_TYPE_VISION_LLM)
        || (pendingRequestIdValue.toInt() != requestId))
    {
        emit LogMessage(QStringLiteral("Agent Vision LLM response ignored because pending state does not match."));
        return;
    }

    if (!m_context.SetValue(CONTEXT_KEY_VISION_ANALYSIS, normalizedContent))
    {
        emit LogMessage(QStringLiteral("Agent failed to record Vision LLM analysis."));
        return;
    }

    if (!m_context.SetValue(CONTEXT_KEY_SEMANTIC_VISION_SUMMARY, normalizedContent))
    {
        emit LogMessage(QStringLiteral("Agent failed to record semantic vision summary."));
        return;
    }

    if (!m_context.SetValue(CONTEXT_KEY_VISION_LLM_PENDING, false))
    {
        emit LogMessage(QStringLiteral("Agent failed to clear Vision LLM pending state."));
        return;
    }

    ClearAsyncPendingState(m_context);
    emit LogMessage(QStringLiteral("Agent Vision LLM analysis completed."));

    if (m_pendingResumeIndex >= 0)
    {
        QString errorMessage;

        if (!ExecuteFromIndex(m_pendingResumeIndex, false, errorMessage))
        {
            m_pendingResumeIndex = -1;
            m_pendingNodeType.clear();
            m_pendingRequestId = -1;
            emit LogMessage(errorMessage);
            return;
        }
    }
}

void AgentRuntime::OnVisionAnalysisFailed(int requestId, const QString &message, int statusCode)
{
    (void)statusCode;

    const QString normalizedMessage = message.trimmed();
    const QString outputMessage = normalizedMessage.isEmpty()
                                  ? QStringLiteral("Agent Vision LLM request failed.")
                                  : normalizedMessage;

    m_context.SetValue(CONTEXT_KEY_VISION_LLM_PENDING, false);
    ClearAsyncPendingState(m_context);
    emit LogMessage(QStringLiteral("Agent Vision LLM request failed: %1 %2").arg(
                        requestId).arg(outputMessage));
}

} // namespace vpet
