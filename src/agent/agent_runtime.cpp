#include "vpet/agent/agent_runtime.h"
#include "vpet/agent/agent_context_keys.h"
#include "vpet/agent/emotion_rewrite_node.h"
#include "vpet/agent/proactive_topic_node.h"
#include "vpet/agent/agent_output_policy.h"
#include "vpet/llm/llm_client.h"
#include "vpet/llm/vision_llm_client.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSize>
#include <QStringList>
#include <QTimer>
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
const QString &NODE_TYPE_PROACTIVE_TOPIC = AgentContextKeys::NODE_TYPE_PROACTIVE_TOPIC;
const QString &NODE_TYPE_USER_INPUT = AgentContextKeys::NODE_TYPE_USER_INPUT;
const QString &CONTEXT_KEY_CONVERSATION_HISTORY = AgentContextKeys::CONVERSATION_HISTORY;
const QString &CONTEXT_KEY_EMOTION_OUTPUT_TEXT = AgentContextKeys::EMOTION_OUTPUT_TEXT;
const QString &CONTEXT_KEY_INPUT_AVAILABLE = AgentContextKeys::INPUT_AVAILABLE;
const QString &CONTEXT_KEY_NODE_INPUT_PROMPT = AgentContextKeys::NODE_INPUT_PROMPT;
const QString &CONTEXT_KEY_NODE_INPUT_TEXT_RESPONSE = AgentContextKeys::NODE_INPUT_TEXT_RESPONSE;
const QString &CONTEXT_KEY_NODE_OUTPUT_TEXT_FINAL = AgentContextKeys::NODE_OUTPUT_TEXT_FINAL;
const QString &CONTEXT_KEY_NODE_OUTPUT_TEXT_RESPONSE = AgentContextKeys::NODE_OUTPUT_TEXT_RESPONSE;
const QString &CONTEXT_KEY_VISION_INPUT_READY = AgentContextKeys::VISION_INPUT_READY;
const QString &CONTEXT_KEY_PROMPT_TEXT = AgentContextKeys::PROMPT_TEXT;
const QString &CONTEXT_KEY_SEMANTIC_OUTPUT_SOURCE = AgentContextKeys::SEMANTIC_OUTPUT_SOURCE;
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
const QString &CONTEXT_KEY_RUNTIME_TRIGGER_TYPE = AgentContextKeys::RUNTIME_TRIGGER_TYPE;
const QString &CONTEXT_KEY_RUNTIME_LAST_NODE_TYPE = AgentContextKeys::RUNTIME_LAST_NODE_TYPE;
const QString &CONTEXT_KEY_RUNTIME_PASS_THROUGH_PREFIX = AgentContextKeys::RUNTIME_PASS_THROUGH_PREFIX;
const QString &CONTEXT_KEY_SEMANTIC_IMAGE_BASE64 = AgentContextKeys::SEMANTIC_IMAGE_BASE64;
const QString &CONTEXT_KEY_SEMANTIC_IMAGE_HEIGHT = AgentContextKeys::SEMANTIC_IMAGE_HEIGHT;
const QString &CONTEXT_KEY_SEMANTIC_IMAGE_MEDIA_TYPE = AgentContextKeys::SEMANTIC_IMAGE_MEDIA_TYPE;
const QString &CONTEXT_KEY_SEMANTIC_IMAGE_WIDTH = AgentContextKeys::SEMANTIC_IMAGE_WIDTH;
const QString &CONTEXT_KEY_SEMANTIC_VISION_FRAME_ID = AgentContextKeys::SEMANTIC_VISION_FRAME_ID;
const QString &CONTEXT_KEY_SEMANTIC_VISION_STATE = AgentContextKeys::SEMANTIC_VISION_STATE;
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
constexpr int DEFAULT_ASYNC_TIMEOUT_MS = 120000;
constexpr int MAX_ASYNC_TIMEOUT_MS = 600000;
const QString TRIGGER_TYPE_USER = QStringLiteral("user");
const QString TRIGGER_TYPE_VISION = QStringLiteral("vision");
const QString ASYNC_CLIENT_TEXT = QStringLiteral("text");
const QString ASYNC_CLIENT_VISION = QStringLiteral("vision");
const QString OUTPUT_SOURCE_USER_RESPONSE = QStringLiteral("user_response");
const QString OUTPUT_SOURCE_VISION_PROACTIVE = QStringLiteral("vision_proactive");

QString ResolveVisionMediaType(const QString &modality)
{
    if (modality.contains(QStringLiteral("jpeg"), Qt::CaseInsensitive)
        || modality.contains(QStringLiteral("jpg"), Qt::CaseInsensitive))
    {
        return QStringLiteral("image/jpeg");
    }

    return QStringLiteral("image/png");
}

} // anonymous namespace

AgentRuntime::AgentRuntime(QObject *parent)
    : QObject(parent)
    , m_dagGraph()
    , m_context()
    , m_sessionContext()
    , m_llmClient(new LlmClient(this))
    , m_visionLlmClient(new VisionLlmClient(this))
    , m_nodeHandlers()
    , m_executionOrder()
    , m_invocationState()
    , m_invocationQueue()
    , m_nextInvocationId(0)
    , m_directRequestIds()
    , m_lastPerceptionFrameHash()
    , m_isLoaded(false)
    , m_contextWasQueued(false)
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

    if (m_invocationState.isActive || HasPendingAsyncRequest() || !m_invocationQueue.IsEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime cannot load a DAG during an active invocation.");
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

    if (m_invocationState.isActive || HasPendingAsyncRequest() || !m_invocationQueue.IsEmpty())
    {
        if (m_contextWasQueued)
        {
            m_contextWasQueued = false;
            return true;
        }

        if (!EnqueueInvocation(m_context))
        {
            errorMessage = QStringLiteral("Agent runtime failed to queue invocation.");
            return false;
        }

        m_context = m_sessionContext.Snapshot();
        return true;
    }

    QVariant triggerValue;

    if (!m_context.GetValue(CONTEXT_KEY_RUNTIME_TRIGGER_TYPE, triggerValue)
        || triggerValue.toString().trimmed().isEmpty())
    {
        m_context = m_sessionContext.Snapshot();
    }

    if (!BeginInvocation(errorMessage))
    {
        return false;
    }

    return PumpReadyQueue(true, errorMessage);
}

bool AgentRuntime::ExecuteWithUserInput(const QString &userInput, QString &errorMessage)
{
    const QString normalizedUserInput = userInput.trimmed();

    m_contextWasQueued = false;

    if (normalizedUserInput.isEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime user input is empty.");
        return false;
    }

    m_context = m_sessionContext.Snapshot();

    if (!m_context.SetUserInput(normalizedUserInput))
    {
        errorMessage = QStringLiteral("Agent runtime failed to set user input.");
        return false;
    }

    if (!m_context.SetValue(CONTEXT_KEY_RUNTIME_TRIGGER_TYPE, TRIGGER_TYPE_USER))
    {
        m_context.RemoveValue(AgentContextKeys::USER_INPUT);
        errorMessage = QStringLiteral("Agent runtime failed to set user trigger type.");
        return false;
    }

    if (m_isLoaded
        && (m_invocationState.isActive
            || HasPendingAsyncRequest()
            || !m_invocationQueue.IsEmpty()))
    {
        if (!EnqueueInvocation(m_context))
        {
            errorMessage = QStringLiteral("Agent runtime failed to queue user invocation.");
            return false;
        }

        m_contextWasQueued = true;
        m_context = m_sessionContext.Snapshot();
        return true;
    }

    if (m_isLoaded)
    {
        if (!Execute(errorMessage))
        {
            ClearInvocationInputState(m_context);
            return false;
        }

        return true;
    }

    emit LogMessage(QStringLiteral("Agent DAG is not loaded. Voice text will use direct LLM fallback."));

    if (!PrepareTextInputContext(m_context, errorMessage))
    {
        ClearInvocationInputState(m_context);
        return false;
    }

    if (!SendUserInputToLlm(normalizedUserInput, errorMessage))
    {
        ClearInvocationInputState(m_context);
        return false;
    }

    return true;
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
    const QString frameHash = QString::fromLatin1(
        QCryptographicHash::hash(encodedData, QCryptographicHash::Sha256).toHex());

    m_contextWasQueued = false;

    if (encodedData.isEmpty())
    {
        errorMessage = QStringLiteral("Agent perception frame data is empty.");
        return false;
    }

    QVariant previousFrameHashValue;

    if ((m_lastPerceptionFrameHash == frameHash)
        || (m_context.GetValue(AgentContextKeys::SEMANTIC_VISION_FRAME_HASH,
                               previousFrameHashValue)
            && (previousFrameHashValue.toString() == frameHash)))
    {
        emit LogMessage(QStringLiteral("Agent perception frame skipped because content is unchanged."));
        return true;
    }

    m_context = m_sessionContext.Snapshot();

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

    const QString base64Image = QString::fromLatin1(encodedData);
    const QString mediaType = ResolveVisionMediaType(normalizedModality);
    m_lastPerceptionFrameHash = frameHash;

    if (!m_context.SetValue(CONTEXT_KEY_VISION_LATEST_BASE64, base64Image)
        || !m_context.SetValue(CONTEXT_KEY_SEMANTIC_IMAGE_BASE64, base64Image)
        || !m_context.SetValue(AgentContextKeys::SEMANTIC_VISION_FRAME_HASH, frameHash))
    {
        errorMessage = QStringLiteral("Agent failed to record latest vision frame.");
        return false;
    }

    if (!m_context.SetValue(CONTEXT_KEY_VISION_LATEST_FRAME_ID, frameId)
        || !m_context.SetValue(CONTEXT_KEY_SEMANTIC_VISION_FRAME_ID, frameId))
    {
        errorMessage = QStringLiteral("Agent failed to record latest vision frame ID.");
        return false;
    }

    if (!m_context.SetValue(CONTEXT_KEY_VISION_LATEST_WIDTH, frameSize.width())
        || !m_context.SetValue(CONTEXT_KEY_SEMANTIC_IMAGE_WIDTH, frameSize.width()))
    {
        errorMessage = QStringLiteral("Agent failed to record latest vision frame width.");
        return false;
    }

    if (!m_context.SetValue(CONTEXT_KEY_VISION_LATEST_HEIGHT, frameSize.height())
        || !m_context.SetValue(CONTEXT_KEY_SEMANTIC_IMAGE_HEIGHT, frameSize.height()))
    {
        errorMessage = QStringLiteral("Agent failed to record latest vision frame height.");
        return false;
    }

    if (!m_context.SetValue(CONTEXT_KEY_VISION_LATEST_MODALITY, normalizedModality))
    {
        errorMessage = QStringLiteral("Agent failed to record latest vision modality.");
        return false;
    }

    if (!m_context.SetValue(CONTEXT_KEY_SEMANTIC_IMAGE_MEDIA_TYPE, mediaType)
        || !m_context.SetValue(CONTEXT_KEY_SEMANTIC_VISION_STATE,
                               QStringLiteral("available")))
    {
        errorMessage = QStringLiteral("Agent failed to record semantic vision state.");
        return false;
    }

    if (!m_context.SetValue(CONTEXT_KEY_VISION_UPDATED_AT,
                            QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)))
    {
        errorMessage = QStringLiteral("Agent failed to record latest vision timestamp.");
        return false;
    }

    if (!m_context.SetValue(CONTEXT_KEY_RUNTIME_TRIGGER_TYPE, TRIGGER_TYPE_VISION))
    {
        errorMessage = QStringLiteral("Agent failed to record vision trigger type.");
        return false;
    }

    m_context.RemoveValue(AgentContextKeys::USER_INPUT);

    if (m_invocationState.isActive || HasPendingAsyncRequest() || !m_invocationQueue.IsEmpty())
    {
        if (!EnqueueInvocation(m_context))
        {
            errorMessage = QStringLiteral("Agent runtime failed to queue vision invocation.");
            return false;
        }

        m_contextWasQueued = true;
        m_context = m_sessionContext.Snapshot();
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

bool AgentRuntime::SetActiveVisionLlmProfile(VISION_LLM_MODEL_PROFILE profile)
{
    if (m_visionLlmClient == nullptr)
    {
        return false;
    }

    return m_visionLlmClient->SetActiveProfile(profile);
}

VISION_LLM_MODEL_PROFILE AgentRuntime::GetActiveVisionLlmProfile() const
{
    if (m_visionLlmClient == nullptr)
    {
        return VISION_LLM_MODEL_PROFILE::GPT;
    }

    return m_visionLlmClient->GetActiveProfile();
}

bool AgentRuntime::HasPendingAsyncRequest() const
{
    return !m_invocationState.pendingByRequestId.isEmpty()
           || !m_directRequestIds.isEmpty();
}

bool AgentRuntime::Start(const QString &configPath, QString &errorMessage)
{
    if (!Load(configPath, errorMessage))
    {
        return false;
    }

    QVariant triggerValue;

    if (!m_context.GetValue(CONTEXT_KEY_RUNTIME_TRIGGER_TYPE, triggerValue)
        || triggerValue.toString().trimmed().isEmpty())
    {
        return true;
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
    m_sessionContext = context.Snapshot();
    m_context = context.Snapshot();
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
        if (context.GetValue(CONTEXT_KEY_NODE_INPUT_PROMPT, value))
        {
            return context.SetValue(CONTEXT_KEY_PROMPT_TEXT, value);
        }

        if (context.GetValue(CONTEXT_KEY_SEMANTIC_TEXT_PROMPT, value))
        {
            return context.SetValue(CONTEXT_KEY_NODE_INPUT_PROMPT, value)
                   && context.SetValue(CONTEXT_KEY_PROMPT_TEXT, value);
        }

        if (context.GetValue(CONTEXT_KEY_PROMPT_TEXT, value))
        {
            return context.SetValue(CONTEXT_KEY_NODE_INPUT_PROMPT, value)
                   && context.SetValue(CONTEXT_KEY_SEMANTIC_TEXT_PROMPT, value);
        }

        return true;
    }

    if (nodeType == NODE_TYPE_EMOTION_REWRITE)
    {
        if (context.GetValue(CONTEXT_KEY_NODE_INPUT_TEXT_RESPONSE, value))
        {
            return context.SetValue(CONTEXT_KEY_SEMANTIC_TEXT_RESPONSE, value)
                   && context.SetValue(AgentContextKeys::LLM_LAST_RESPONSE, value);
        }

        if (context.GetValue(CONTEXT_KEY_SEMANTIC_TEXT_RESPONSE, value))
        {
            return context.SetValue(CONTEXT_KEY_NODE_INPUT_TEXT_RESPONSE, value)
                   && context.SetValue(AgentContextKeys::LLM_LAST_RESPONSE, value);
        }

        if (context.GetValue(AgentContextKeys::LLM_LAST_RESPONSE, value))
        {
            return context.SetValue(CONTEXT_KEY_NODE_INPUT_TEXT_RESPONSE, value)
                   && context.SetValue(CONTEXT_KEY_SEMANTIC_TEXT_RESPONSE, value);
        }

        return true;
    }

    if (nodeType == NODE_TYPE_OUTPUT_FORMAT)
    {
        if (context.GetValue(CONTEXT_KEY_SEMANTIC_TEXT_RESPONSE, value)
            || context.GetValue(CONTEXT_KEY_NODE_OUTPUT_TEXT_RESPONSE, value)
            || context.GetValue(CONTEXT_KEY_EMOTION_OUTPUT_TEXT, value)
            || context.GetValue(AgentContextKeys::LLM_LAST_RESPONSE, value)
            || context.GetValue(CONTEXT_KEY_NODE_INPUT_TEXT_RESPONSE, value))
        {
            return context.SetValue(CONTEXT_KEY_SEMANTIC_TEXT_RESPONSE, value)
                   && context.SetValue(CONTEXT_KEY_NODE_INPUT_TEXT_RESPONSE, value);
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

bool AgentRuntime::BeginInvocation(QString &errorMessage)
{
    if (!m_isLoaded)
    {
        errorMessage = QStringLiteral("Agent runtime is not loaded.");
        return false;
    }

    if (m_invocationState.isActive || HasPendingAsyncRequest())
    {
        errorMessage = QStringLiteral("Agent runtime invocation is already active.");
        return false;
    }

    const QVector<QString> allSourceNodes = m_dagGraph.GetSourceNodes();

    if (allSourceNodes.isEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime DAG has no source node.");
        return false;
    }

    m_invocationState.remainingInDegree = m_dagGraph.GetInDegreeMap();
    m_invocationState.nodeDeclarationOrder.clear();
    m_invocationState.readyQueue.clear();
    m_invocationState.completedNodeIds.clear();
    m_invocationState.nodeExecutionResults.clear();
    m_invocationState.branches.clear();
    m_invocationState.nodeBranchIds.clear();
    m_invocationState.nodeResults.clear();
    m_invocationState.pendingByRequestId.clear();
    m_invocationState.activeNodeIds.clear();
    m_invocationState.failureMessage.clear();
    m_invocationState.hasFailed = false;

    const QVector<QString> nodeNames = m_dagGraph.GetNodeNames();

    if (m_invocationState.remainingInDegree.size() != nodeNames.size())
    {
        errorMessage = QStringLiteral("Agent runtime DAG invocation state is incomplete.");
        return false;
    }

    for (int nodeIndex = 0; nodeIndex < nodeNames.size(); ++nodeIndex)
    {
        const QString &nodeId = nodeNames.at(nodeIndex);
        m_invocationState.nodeDeclarationOrder.insert(nodeId, nodeIndex);
    }

    QVariant triggerValue;

    if (m_context.GetValue(CONTEXT_KEY_RUNTIME_TRIGGER_TYPE, triggerValue))
    {
        m_invocationState.trigger = triggerValue.toString().trimmed();
    }
    else
    {
        m_invocationState.trigger.clear();
    }

    ++m_nextInvocationId;

    if (m_nextInvocationId == 0)
    {
        ++m_nextInvocationId;
    }

    m_invocationState.invocationId = m_nextInvocationId;

    QVector<QString> sourceNodes;
    QVector<QString> matchingSourceNodes;
    bool hasDeclaredSourceTrigger = false;

    for (const QString &sourceNode : allSourceNodes)
    {
        _tagAgentDagNode sourceDefinition;

        if (!m_dagGraph.GetNode(sourceNode, sourceDefinition))
        {
            errorMessage = QStringLiteral("Agent runtime source node definition is missing: %1")
                               .arg(sourceNode);
            ClearInvocationState();
            return false;
        }

        const QString sourceTrigger = sourceDefinition.config.value(QStringLiteral("trigger"))
                                           .toString()
                                           .trimmed();

        if (!sourceTrigger.isEmpty())
        {
            hasDeclaredSourceTrigger = true;
        }

        if (!m_invocationState.trigger.isEmpty() && sourceTrigger == m_invocationState.trigger)
        {
            matchingSourceNodes.append(sourceNode);
        }
    }

    if (hasDeclaredSourceTrigger && matchingSourceNodes.isEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime DAG has no source for trigger: %1").arg(
                           m_invocationState.trigger);
        ClearInvocationState();
        return false;
    }

    sourceNodes = hasDeclaredSourceTrigger ? matchingSourceNodes : allSourceNodes;

    QQueue<QString> reachableQueue;

    for (const QString &sourceNode : sourceNodes)
    {
        reachableQueue.enqueue(sourceNode);
    }

    while (!reachableQueue.isEmpty())
    {
        const QString nodeId = reachableQueue.dequeue();

        if (m_invocationState.activeNodeIds.contains(nodeId))
        {
            continue;
        }

        m_invocationState.activeNodeIds.insert(nodeId);
        QVector<QString> successors;

        if (!m_dagGraph.GetSuccessors(nodeId, successors))
        {
            errorMessage = QStringLiteral("Agent runtime reachable node lookup failed: %1").arg(nodeId);
            ClearInvocationState();
            return false;
        }

        for (const QString &successor : successors)
        {
            reachableQueue.enqueue(successor);
        }
    }

    for (const QString &nodeId : nodeNames)
    {
        if (!m_invocationState.activeNodeIds.contains(nodeId))
        {
            m_invocationState.remainingInDegree.remove(nodeId);
            continue;
        }

        QVector<QString> predecessors;

        if (!m_dagGraph.GetPredecessors(nodeId, predecessors))
        {
            errorMessage = QStringLiteral("Agent runtime active node predecessors are missing: %1")
                               .arg(nodeId);
            ClearInvocationState();
            return false;
        }

        int activeInDegree = 0;

        for (const QString &predecessor : predecessors)
        {
            if (m_invocationState.activeNodeIds.contains(predecessor))
            {
                ++activeInDegree;
            }
        }

        m_invocationState.remainingInDegree.insert(nodeId, activeInDegree);
    }

    for (const QString &sourceNode : sourceNodes)
    {
        _tagInvocationState::_tagBranchState branch;
        _tagAgentDagNode sourceNodeDefinition;

        if (!m_dagGraph.GetNode(sourceNode, sourceNodeDefinition))
        {
            errorMessage = QStringLiteral("Agent runtime source node definition is missing: %1").arg(
                               sourceNode);
            ClearInvocationState();
            return false;
        }

        branch.branchId = sourceNode;
        branch.sourceNodeId = sourceNode;
        branch.sourceTrigger = sourceNodeDefinition.config.value(QStringLiteral("trigger"))
                                   .toString()
                                   .trimmed();
        m_invocationState.branches.insert(branch.branchId, branch);
        m_invocationState.nodeBranchIds.insert(sourceNode, branch.branchId);

        if (!EnqueueReadyNode(sourceNode, errorMessage))
        {
            ClearInvocationState();
            return false;
        }
    }

    m_invocationState.isActive = true;
    return true;
}

bool AgentRuntime::EnqueueInvocation(const AgentContext &context)
{
    return m_invocationQueue.Enqueue(context, m_sessionContext);
}

bool AgentRuntime::StartNextQueuedInvocation(QString &errorMessage)
{
    if (m_invocationState.isActive || m_invocationQueue.IsEmpty())
    {
        return true;
    }

    InvocationQueuePolicy::_tagEntry queuedInvocation;

    if (!m_invocationQueue.Dequeue(queuedInvocation))
    {
        return true;
    }
    m_contextWasQueued = false;
    m_context = m_sessionContext.Snapshot();

    if (!m_context.Overlay(queuedInvocation.local))
    {
        errorMessage = QStringLiteral("Agent runtime failed to restore queued invocation input.");
        m_context = m_sessionContext.Snapshot();
        return false;
    }

    for (const QString &removedKey : queuedInvocation.removedKeys)
    {
        m_context.RemoveValue(removedKey);
    }

    if (!BeginInvocation(errorMessage))
    {
        m_context = m_sessionContext.Snapshot();
        return false;
    }

    if (!PumpReadyQueue(true, errorMessage))
    {
        return false;
    }

    return true;
}

bool AgentRuntime::PumpReadyQueue(bool shouldPrepareInput, QString &errorMessage)
{
    if (!m_invocationState.isActive)
    {
        errorMessage = QStringLiteral("Agent runtime invocation is not active.");
        return false;
    }

    if (shouldPrepareInput && !PrepareTextInputContext(m_context, errorMessage))
    {
        ClearInvocationState();
        ClearInvocationInputState(m_context);
        return false;
    }

    if (shouldPrepareInput)
    {
        QVector<QString> sourceNodes;

        for (const QString &nodeId : m_dagGraph.GetNodeNames())
        {
            if (m_invocationState.activeNodeIds.contains(nodeId)
                && m_invocationState.remainingInDegree.value(nodeId) == 0)
            {
                sourceNodes.append(nodeId);
            }
        }

        for (const QString &sourceNode : sourceNodes)
        {
            const QString branchId = m_invocationState.nodeBranchIds.value(sourceNode);

            if (!m_invocationState.branches.contains(branchId))
            {
                errorMessage = QStringLiteral("Agent runtime source branch is missing: %1").arg(sourceNode);
                ResetAsyncExecutionState(m_context);
                return false;
            }

            _tagInvocationState::_tagBranchState &branch = m_invocationState.branches[branchId];

            if (!m_context.BuildDelta(m_sessionContext, branch.local, branch.removedKeys))
            {
                errorMessage = QStringLiteral("Agent runtime failed to initialize source branch context: %1")
                                   .arg(sourceNode);
                ResetAsyncExecutionState(m_context);
                return false;
            }
        }
    }

    while (!m_invocationState.readyQueue.isEmpty())
    {
        const QString nodeId = m_invocationState.readyQueue.takeFirst();
        _tagAgentDagNode node;

        if (!m_invocationState.remainingInDegree.contains(nodeId)
            || (m_invocationState.remainingInDegree.value(nodeId) != 0))
        {
            errorMessage = QStringLiteral("Agent runtime dequeued node before its dependencies completed: %1")
                               .arg(nodeId);
            m_invocationState.hasFailed = true;
            m_invocationState.failureMessage = errorMessage;
            ResetAsyncExecutionState(m_context);
            return false;
        }

        if (m_invocationState.completedNodeIds.contains(nodeId))
        {
            errorMessage = QStringLiteral("Agent runtime dequeued an already completed node: %1").arg(nodeId);
            m_invocationState.hasFailed = true;
            m_invocationState.failureMessage = errorMessage;
            ResetAsyncExecutionState(m_context);
            return false;
        }

        if (!m_dagGraph.GetNode(nodeId, node))
        {
            errorMessage = QStringLiteral("Agent runtime node definition is missing: %1").arg(nodeId);
            m_invocationState.hasFailed = true;
            m_invocationState.failureMessage = errorMessage;
            ResetAsyncExecutionState(m_context);
            return false;
        }

        AgentContext executionContext;

        if (!BuildExecutionView(nodeId, executionContext, errorMessage))
        {
            m_invocationState.hasFailed = true;
            m_invocationState.failureMessage = errorMessage;
            ResetAsyncExecutionState(m_context);
            return false;
        }

        if (!ExecuteNode(node, executionContext, errorMessage))
        {
            m_invocationState.hasFailed = true;
            m_invocationState.failureMessage = errorMessage;
            m_invocationState.nodeExecutionResults.insert(nodeId, false);
            ResetAsyncExecutionState(m_context);
            return false;
        }

        if (!SaveNodeResult(nodeId, executionContext, errorMessage))
        {
            m_invocationState.hasFailed = true;
            m_invocationState.failureMessage = errorMessage;
            m_invocationState.nodeExecutionResults.insert(nodeId, false);
            ResetAsyncExecutionState(m_context);
            return false;
        }

        m_context = executionContext.Snapshot();

        QVariant pendingValue;

        if (executionContext.GetValue(CONTEXT_KEY_RUNTIME_PENDING, pendingValue)
            && pendingValue.toBool())
        {
            if (!RegisterPendingNode(node, executionContext, errorMessage))
            {
                m_invocationState.hasFailed = true;
                m_invocationState.failureMessage = errorMessage;
                ResetAsyncExecutionState(m_context);
                return false;
            }

            qDebug() << "[Agent] Node is waiting for async response:" << nodeId;
            continue;
        }

        if (!CompleteNode(nodeId, errorMessage))
        {
            m_invocationState.hasFailed = true;
            m_invocationState.failureMessage = errorMessage;
            ResetAsyncExecutionState(m_context);
            return false;
        }
    }

    if (!m_invocationState.pendingByRequestId.isEmpty())
    {
        return true;
    }

    if (m_invocationState.completedNodeIds.size() != m_invocationState.remainingInDegree.size())
    {
        errorMessage = QStringLiteral("Agent runtime invocation stopped before all nodes completed.");
        m_invocationState.hasFailed = true;
        m_invocationState.failureMessage = errorMessage;
        ResetAsyncExecutionState(m_context);
        return false;
    }

    if (!CommitInvocationResult(errorMessage))
    {
        m_invocationState.hasFailed = true;
        m_invocationState.failureMessage = errorMessage;
        ResetAsyncExecutionState(m_context);
        return false;
    }

    ClearInvocationState();
    ClearInvocationInputState(m_context);

    if (!m_invocationQueue.IsEmpty())
    {
        QTimer::singleShot(0, this, [this]()
        {
            QString queuedErrorMessage;

            if (!StartNextQueuedInvocation(queuedErrorMessage) && !queuedErrorMessage.isEmpty())
            {
                emit LogMessage(queuedErrorMessage);
            }
        });
    }

    qDebug() << "[Agent] Ready queue execution finished.";

    return true;
}

bool AgentRuntime::CompleteNode(const QString &nodeId, QString &errorMessage)
{
    const QString normalizedNodeId = nodeId.trimmed();

    if (normalizedNodeId.isEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime completed node id is empty.");
        return false;
    }

    if (!m_invocationState.isActive)
    {
        errorMessage = QStringLiteral("Agent runtime cannot complete a node without an active invocation.");
        return false;
    }

    if (!m_invocationState.remainingInDegree.contains(normalizedNodeId))
    {
        errorMessage = QStringLiteral("Agent runtime completed node is missing from invocation: %1").arg(
                           normalizedNodeId);
        return false;
    }

    if (m_invocationState.completedNodeIds.contains(normalizedNodeId))
    {
        errorMessage = QStringLiteral("Agent runtime node was completed more than once: %1").arg(
                           normalizedNodeId);
        return false;
    }

    QVector<QString> successors;

    if (!m_dagGraph.GetSuccessors(normalizedNodeId, successors))
    {
        errorMessage = QStringLiteral("Agent runtime completed node is not in DAG: %1").arg(
                           normalizedNodeId);
        return false;
    }

    m_invocationState.completedNodeIds.insert(normalizedNodeId);
    m_invocationState.nodeExecutionResults.insert(normalizedNodeId, true);

    for (const QString &successorId : successors)
    {
        if (!m_invocationState.remainingInDegree.contains(successorId))
        {
            errorMessage = QStringLiteral("Agent runtime successor is missing from invocation: %1").arg(
                               successorId);
            return false;
        }

        const int remainingInDegree = m_invocationState.remainingInDegree.value(successorId);

        if (remainingInDegree <= 0)
        {
            errorMessage = QStringLiteral("Agent runtime successor was completed more than once: %1").arg(
                               successorId);
            return false;
        }

        const int nextRemainingInDegree = remainingInDegree - 1;
        m_invocationState.remainingInDegree.insert(successorId, nextRemainingInDegree);

        if (nextRemainingInDegree == 0)
        {
            QVector<QString> predecessors;

            if (!m_dagGraph.GetPredecessors(successorId, predecessors))
            {
                errorMessage = QStringLiteral("Agent runtime successor predecessors are missing: %1").arg(
                                   successorId);
                return false;
            }

            int activePredecessorCount = 0;

            for (const QString &predecessor : predecessors)
            {
                if (m_invocationState.activeNodeIds.contains(predecessor))
                {
                    ++activePredecessorCount;
                }
            }

            if (activePredecessorCount > 1)
            {
                if (!CreateJoinBranch(successorId, errorMessage))
                {
                    return false;
                }
            }
            else if (!CreateChildBranch(normalizedNodeId, successorId, errorMessage))
            {
                return false;
            }

            if (!EnqueueReadyNode(successorId, errorMessage))
            {
                return false;
            }
        }
    }

    return true;
}

bool AgentRuntime::BuildExecutionView(const QString &nodeId,
                                      AgentContext &context,
                                      QString &errorMessage) const
{
    const QString normalizedNodeId = nodeId.trimmed();

    if (normalizedNodeId.isEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime execution view node id is empty.");
        return false;
    }

    const QString branchId = m_invocationState.nodeBranchIds.value(normalizedNodeId);

    if (branchId.isEmpty() || !m_invocationState.branches.contains(branchId))
    {
        errorMessage = QStringLiteral("Agent runtime execution branch is missing: %1").arg(normalizedNodeId);
        return false;
    }

    const _tagInvocationState::_tagBranchState &branch = m_invocationState.branches.value(branchId);

    context = m_sessionContext.Snapshot();

    if (!context.Overlay(branch.local))
    {
        errorMessage = QStringLiteral("Agent runtime failed to overlay branch local context: %1").arg(
                           normalizedNodeId);
        return false;
    }

    for (const QString &removedKey : branch.removedKeys)
    {
        context.RemoveValue(removedKey);
    }

    return true;
}

bool AgentRuntime::SaveNodeResult(const QString &nodeId,
                                  const AgentContext &afterContext,
                                  QString &errorMessage)
{
    const QString normalizedNodeId = nodeId.trimmed();

    if (normalizedNodeId.isEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime node result id is empty.");
        return false;
    }

    const QString branchId = m_invocationState.nodeBranchIds.value(normalizedNodeId);

    if (branchId.isEmpty() || !m_invocationState.branches.contains(branchId))
    {
        errorMessage = QStringLiteral("Agent runtime node result branch is missing: %1").arg(
                           normalizedNodeId);
        return false;
    }

    _tagInvocationState::_tagBranchState &branch = m_invocationState.branches[branchId];

    if (!afterContext.BuildDelta(m_sessionContext, branch.local, branch.removedKeys))
    {
        errorMessage = QStringLiteral("Agent runtime failed to save branch context delta: %1").arg(
                           normalizedNodeId);
        return false;
    }

    _tagInvocationState::_tagNodeResult result;

    result.branchId = branchId;
    result.sourceNodeId = branch.sourceNodeId;
    result.sourceTrigger = branch.sourceTrigger;
    result.local = branch.local.Snapshot();
    result.removedKeys = branch.removedKeys;
    m_invocationState.nodeResults.insert(normalizedNodeId, result);

    return true;
}

bool AgentRuntime::CreateChildBranch(const QString &parentNodeId,
                                     const QString &childNodeId,
                                     QString &errorMessage)
{
    const QString normalizedParentNodeId = parentNodeId.trimmed();
    const QString normalizedChildNodeId = childNodeId.trimmed();

    if (normalizedParentNodeId.isEmpty() || normalizedChildNodeId.isEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime child branch node id is empty.");
        return false;
    }

    if (m_invocationState.nodeBranchIds.contains(normalizedChildNodeId))
    {
        errorMessage = QStringLiteral("Agent runtime child node already has a branch: %1").arg(
                           normalizedChildNodeId);
        return false;
    }

    if (!m_invocationState.nodeResults.contains(normalizedParentNodeId))
    {
        errorMessage = QStringLiteral("Agent runtime parent node result is missing: %1").arg(
                           normalizedParentNodeId);
        return false;
    }

    const _tagInvocationState::_tagNodeResult parentResult =
        m_invocationState.nodeResults.value(normalizedParentNodeId);
    _tagInvocationState::_tagBranchState branch;

    branch.branchId = normalizedChildNodeId;
    branch.sourceNodeId = parentResult.sourceNodeId;
    branch.sourceTrigger = parentResult.sourceTrigger;
    branch.local = parentResult.local.Snapshot();
    branch.removedKeys = parentResult.removedKeys;
    m_invocationState.branches.insert(branch.branchId, branch);
    m_invocationState.nodeBranchIds.insert(normalizedChildNodeId, branch.branchId);

    return true;
}

bool AgentRuntime::CreateJoinBranch(const QString &childNodeId, QString &errorMessage)
{
    const QString normalizedChildNodeId = childNodeId.trimmed();

    if (normalizedChildNodeId.isEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime join node id is empty.");
        return false;
    }

    if (m_invocationState.nodeBranchIds.contains(normalizedChildNodeId))
    {
        errorMessage = QStringLiteral("Agent runtime join node already has a branch: %1").arg(
                           normalizedChildNodeId);
        return false;
    }

    QVector<QString> predecessors;
    _tagAgentDagNode joinNode;

    if (!m_dagGraph.GetPredecessors(normalizedChildNodeId, predecessors)
        || !m_dagGraph.GetNode(normalizedChildNodeId, joinNode))
    {
        errorMessage = QStringLiteral("Agent runtime join definition is invalid: %1").arg(
                           normalizedChildNodeId);
        return false;
    }

    QVector<QString> activePredecessors;

    for (const QString &predecessor : predecessors)
    {
        if (m_invocationState.activeNodeIds.contains(predecessor))
        {
            activePredecessors.append(predecessor);
        }
    }

    predecessors = activePredecessors;

    if (predecessors.size() < 2)
    {
        errorMessage = QStringLiteral("Agent runtime join has fewer than two active parents: %1").arg(
                           normalizedChildNodeId);
        return false;
    }

    const QJsonValue mergeValue = joinNode.config.value(QStringLiteral("merge"));

    if (!mergeValue.isUndefined() && !mergeValue.isObject())
    {
        errorMessage = QStringLiteral("Agent runtime join merge config must be an object: %1").arg(
                           normalizedChildNodeId);
        return false;
    }

    const QJsonObject mergeRules = mergeValue.toObject();
    QSet<QString> candidateKeys;

    for (const QString &predecessorId : predecessors)
    {
        if (!m_invocationState.nodeResults.contains(predecessorId))
        {
            errorMessage = QStringLiteral("Agent runtime join parent result is missing: %1 -> %2")
                               .arg(predecessorId, normalizedChildNodeId);
            return false;
        }

        const _tagInvocationState::_tagNodeResult &parentResult =
            m_invocationState.nodeResults[predecessorId];

        for (const QString &key : parentResult.local.GetKeys())
        {
            candidateKeys.insert(key);
        }

        candidateKeys.unite(parentResult.removedKeys);
    }

    _tagInvocationState::_tagBranchState branch;
    branch.branchId = normalizedChildNodeId;

    if (!m_invocationState.trigger.isEmpty()
        && !branch.local.SetValue(AgentContextKeys::RUNTIME_TRIGGER_TYPE,
                                  m_invocationState.trigger))
    {
        errorMessage = QStringLiteral("Agent runtime failed to restore join trigger: %1").arg(
                           normalizedChildNodeId);
        return false;
    }

    QStringList sortedKeys = candidateKeys.values();
    sortedKeys.sort();

    for (const QString &key : sortedKeys)
    {
        if (!MergeJoinKey(normalizedChildNodeId,
                          key,
                          predecessors,
                          mergeRules,
                          branch.local,
                          branch.removedKeys,
                          errorMessage))
        {
            return false;
        }
    }

    m_invocationState.branches.insert(branch.branchId, branch);
    m_invocationState.nodeBranchIds.insert(normalizedChildNodeId, branch.branchId);
    return true;
}

bool AgentRuntime::MergeJoinKey(const QString &joinNodeId,
                                const QString &key,
                                const QVector<QString> &predecessors,
                                const QJsonObject &mergeRules,
                                AgentContext &local,
                                QSet<QString> &removedKeys,
                                QString &errorMessage)
{
    if (joinNodeId.trimmed().isEmpty() || key.trimmed().isEmpty() || predecessors.isEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime join key arguments are invalid.");
        return false;
    }

    if (key == AgentContextKeys::EXECUTED_NODES)
    {
        QStringList mergedExecutedNodes;

        for (const QString &predecessorId : predecessors)
        {
            QVariant value;

            if (!m_invocationState.nodeResults[predecessorId].local.GetValue(key, value))
            {
                continue;
            }

            for (const QString &executedNodeId : value.toStringList())
            {
                if (!mergedExecutedNodes.contains(executedNodeId))
                {
                    mergedExecutedNodes.append(executedNodeId);
                }
            }
        }

        return local.SetValue(key, mergedExecutedNodes);
    }

    if (key.startsWith(QStringLiteral("runtime.")))
    {
        return true;
    }

    QVector<QString> candidateParents;
    QVector<QVariant> candidateValues;
    QVector<bool> candidateRemovals;

    for (const QString &predecessorId : predecessors)
    {
        const _tagInvocationState::_tagNodeResult &parentResult =
            m_invocationState.nodeResults[predecessorId];
        QVariant value;

        if (parentResult.local.GetValue(key, value))
        {
            candidateParents.append(predecessorId);
            candidateValues.append(value);
            candidateRemovals.append(false);
        }
        else if (parentResult.removedKeys.contains(key))
        {
            candidateParents.append(predecessorId);
            candidateValues.append(QVariant());
            candidateRemovals.append(true);
        }
    }

    if (candidateParents.isEmpty())
    {
        return true;
    }

    const QString strategy = mergeRules.value(key).toString().trimmed().toLower();

    if (!strategy.isEmpty()
        && (strategy != QStringLiteral("prefer_user"))
        && (strategy != QStringLiteral("prefer_vision"))
        && (strategy != QStringLiteral("concat")))
    {
        errorMessage = QStringLiteral("Agent runtime join strategy is invalid at node %1 for key %2: %3")
                           .arg(joinNodeId, key, strategy);
        return false;
    }

    QVector<int> selectedIndices;

    for (int candidateIndex = 0; candidateIndex < candidateParents.size(); ++candidateIndex)
    {
        if ((strategy == QStringLiteral("prefer_user"))
            || (strategy == QStringLiteral("prefer_vision")))
        {
            const QString preferredTrigger = (strategy == QStringLiteral("prefer_user"))
                                                 ? QStringLiteral("user")
                                                 : QStringLiteral("vision");
            const _tagInvocationState::_tagNodeResult &parentResult =
                m_invocationState.nodeResults[candidateParents.at(candidateIndex)];

            if (parentResult.sourceTrigger != preferredTrigger)
            {
                continue;
            }
        }

        selectedIndices.append(candidateIndex);
    }

    if (selectedIndices.isEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime join strategy %1 found no matching parent for key %2 at node %3.")
                           .arg(strategy, key, joinNodeId);
        return false;
    }

    if (strategy == QStringLiteral("concat"))
    {
        QStringList values;

        for (const int selectedIndex : selectedIndices)
        {
            const QVariant &value = candidateValues.at(selectedIndex);

            if (candidateRemovals.at(selectedIndex)
                || ((value.metaType().id() != QMetaType::QString)
                    && (value.metaType().id() != QMetaType::QStringList)))
            {
                errorMessage = QStringLiteral("Agent runtime join concat requires string values for key %1 at node %2.")
                                   .arg(key, joinNodeId);
                return false;
            }

            values.append(value.metaType().id() == QMetaType::QStringList
                              ? value.toStringList()
                              : QStringList({value.toString()}));
        }

        return local.SetValue(key, values.join(QStringLiteral("\n")));
    }

    const int firstIndex = selectedIndices.first();
    const bool isRemoved = candidateRemovals.at(firstIndex);
    const QVariant selectedValue = candidateValues.at(firstIndex);

    for (const int selectedIndex : selectedIndices)
    {
        if ((candidateRemovals.at(selectedIndex) != isRemoved)
            || (!isRemoved && (candidateValues.at(selectedIndex) != selectedValue)))
        {
            errorMessage = QStringLiteral("Agent runtime join conflict at node %1 for key %2 from parents %3.")
                               .arg(joinNodeId,
                                    key,
                                    candidateParents.join(QStringLiteral(", ")));
            return false;
        }
    }

    if (isRemoved)
    {
        removedKeys.insert(key);
        return true;
    }

    return local.SetValue(key, selectedValue);
}

bool AgentRuntime::CommitInvocationResult(QString &errorMessage)
{
    if (m_invocationState.completedNodeIds.isEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime invocation completed without nodes.");
        return false;
    }

    QString terminalNodeId;

    for (const QString &nodeId : m_dagGraph.GetNodeNames())
    {
        if (!m_invocationState.activeNodeIds.contains(nodeId))
        {
            continue;
        }

        QVector<QString> successors;

        if (!m_dagGraph.GetSuccessors(nodeId, successors))
        {
            errorMessage = QStringLiteral("Agent runtime terminal node lookup failed: %1").arg(nodeId);
            return false;
        }

        bool hasActiveSuccessor = false;

        for (const QString &successor : successors)
        {
            if (m_invocationState.activeNodeIds.contains(successor))
            {
                hasActiveSuccessor = true;
                break;
            }
        }

        if (!hasActiveSuccessor)
        {
            if (!terminalNodeId.isEmpty())
            {
                errorMessage = QStringLiteral(
                                   "Agent runtime has multiple terminal branches; P3 join merging is required.");
                return false;
            }

            terminalNodeId = nodeId;
        }
    }

    if (terminalNodeId.isEmpty() || !m_invocationState.nodeResults.contains(terminalNodeId))
    {
        errorMessage = QStringLiteral("Agent runtime terminal node result is missing.");
        return false;
    }

    const _tagInvocationState::_tagNodeResult terminalResult =
        m_invocationState.nodeResults.value(terminalNodeId);
    QVariant historyValue;

    if (terminalResult.local.GetValue(CONTEXT_KEY_CONVERSATION_HISTORY, historyValue)
        && !m_sessionContext.SetValue(CONTEXT_KEY_CONVERSATION_HISTORY, historyValue))
    {
        errorMessage = QStringLiteral("Agent runtime failed to commit conversation history.");
        return false;
    }

    if (terminalResult.removedKeys.contains(CONTEXT_KEY_CONVERSATION_HISTORY))
    {
        m_sessionContext.RemoveValue(CONTEXT_KEY_CONVERSATION_HISTORY);
    }

    const QStringList persistentKeys =
    {
        AgentContextKeys::PROACTIVE_LAST_SPOKEN_AT,
        AgentContextKeys::PROACTIVE_LAST_SUMMARY_HASH,
        AgentContextKeys::SEMANTIC_VISION_FRAME_HASH
    };

    for (const QString &key : persistentKeys)
    {
        QVariant value;

        if (terminalResult.local.GetValue(key, value)
            && !m_sessionContext.SetValue(key, value))
        {
            errorMessage = QStringLiteral("Agent runtime failed to commit persistent key: %1")
                               .arg(key);
            return false;
        }
    }

    return BuildExecutionView(terminalNodeId, m_context, errorMessage);
}

bool AgentRuntime::EnqueueReadyNode(const QString &nodeId, QString &errorMessage)
{
    const QString normalizedNodeId = nodeId.trimmed();

    if (normalizedNodeId.isEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime ready node id is empty.");
        return false;
    }

    if (!m_invocationState.nodeDeclarationOrder.contains(normalizedNodeId))
    {
        errorMessage = QStringLiteral("Agent runtime ready node is missing from declaration order: %1").arg(
                           normalizedNodeId);
        return false;
    }

    if (m_invocationState.readyQueue.contains(normalizedNodeId)
        || m_invocationState.completedNodeIds.contains(normalizedNodeId))
    {
        errorMessage = QStringLiteral("Agent runtime ready node was queued more than once: %1").arg(
                           normalizedNodeId);
        return false;
    }

    const int declarationOrder = m_invocationState.nodeDeclarationOrder.value(normalizedNodeId);
    int insertionIndex = 0;

    while ((insertionIndex < m_invocationState.readyQueue.size())
           && (m_invocationState.nodeDeclarationOrder.value(
                   m_invocationState.readyQueue.at(insertionIndex)) < declarationOrder))
    {
        ++insertionIndex;
    }

    m_invocationState.readyQueue.insert(insertionIndex, normalizedNodeId);
    return true;
}

bool AgentRuntime::RegisterPendingNode(const _tagAgentDagNode &node,
                                       const AgentContext &context,
                                       QString &errorMessage)
{
    QVariant requestIdValue;
    const QString nodeId = node.id.trimmed();
    const QString nodeType = node.type.trimmed();

    if (!m_invocationState.isActive || nodeId.isEmpty() || nodeType.isEmpty()
        || !context.GetValue(CONTEXT_KEY_RUNTIME_PENDING_REQUEST_ID, requestIdValue))
    {
        errorMessage = QStringLiteral("Agent runtime pending node state is invalid.");
        return false;
    }

    const int requestId = requestIdValue.toInt();
    const QString branchId = m_invocationState.nodeBranchIds.value(nodeId);
    const QString clientType = (nodeType == NODE_TYPE_VISION_LLM)
                                   ? ASYNC_CLIENT_VISION
                                   : ASYNC_CLIENT_TEXT;
    const QString pendingKey = BuildPendingRequestKey(clientType, requestId);
    const int timeoutMs = node.config.value(QStringLiteral("async_timeout_ms"))
                              .toInt(DEFAULT_ASYNC_TIMEOUT_MS);

    if ((requestId <= 0) || branchId.isEmpty() || pendingKey.isEmpty()
        || m_invocationState.pendingByRequestId.contains(pendingKey)
        || (timeoutMs <= 0) || (timeoutMs > MAX_ASYNC_TIMEOUT_MS))
    {
        errorMessage = QStringLiteral("Agent runtime pending request is invalid, duplicated, or has an invalid timeout: %1")
                           .arg(requestId);
        return false;
    }

    _tagInvocationState::_tagPendingRequest pendingRequest;
    pendingRequest.requestId = requestId;
    pendingRequest.clientType = clientType;
    pendingRequest.invocationId = m_invocationState.invocationId;
    pendingRequest.nodeId = nodeId;
    pendingRequest.branchId = branchId;
    pendingRequest.nodeType = nodeType;
    pendingRequest.context = context.Snapshot();
    m_invocationState.pendingByRequestId.insert(pendingKey, pendingRequest);

    const quint64 invocationId = m_invocationState.invocationId;
    QTimer::singleShot(timeoutMs, this, [this, pendingKey, requestId, invocationId]()
    {
        HandlePendingRequestTimeout(pendingKey, requestId, invocationId);
    });

    return true;
}

QString AgentRuntime::BuildPendingRequestKey(const QString &clientType, int requestId) const
{
    const QString normalizedClientType = clientType.trimmed().toLower();

    if (normalizedClientType.isEmpty() || (requestId <= 0))
    {
        return QString();
    }

    return QStringLiteral("%1:%2").arg(normalizedClientType).arg(requestId);
}

bool AgentRuntime::ResumePendingNode(const QString &pendingKey,
                                     int requestId,
                                     const AgentContext &context,
                                     QString &errorMessage)
{
    if (pendingKey.trimmed().isEmpty() || (requestId <= 0) || !m_invocationState.isActive
        || !m_invocationState.pendingByRequestId.contains(pendingKey))
    {
        errorMessage = QStringLiteral("Agent runtime has no matching pending request to resume.");
        return false;
    }

    const _tagInvocationState::_tagPendingRequest pendingRequest =
        m_invocationState.pendingByRequestId.take(pendingKey);

    if (pendingRequest.invocationId != m_invocationState.invocationId)
    {
        errorMessage = QStringLiteral("Agent runtime pending request belongs to an old invocation.");
        return false;
    }

    m_context = context.Snapshot();

    if (!SaveNodeResult(pendingRequest.nodeId, context, errorMessage)
        || !CompleteNode(pendingRequest.nodeId, errorMessage))
    {
        return false;
    }

    return PumpReadyQueue(false, errorMessage);
}

void AgentRuntime::HandlePendingRequestTimeout(const QString &pendingKey,
                                               int requestId,
                                               quint64 invocationId)
{
    if (pendingKey.trimmed().isEmpty() || (requestId <= 0) || (invocationId == 0)
        || !m_invocationState.isActive
        || (m_invocationState.invocationId != invocationId)
        || !m_invocationState.pendingByRequestId.contains(pendingKey))
    {
        return;
    }

    ResetAsyncExecutionState(m_context);
    const QString message = QStringLiteral("Agent async request timed out.");
    emit LogMessage(QStringLiteral("%1 Request ID: %2").arg(message).arg(requestId));
    emit LlmRequestFailed(requestId, message, 0);
}

void AgentRuntime::ClearInvocationState()
{
    m_invocationState.isActive = false;
    m_invocationState.hasFailed = false;
    m_invocationState.invocationId = 0;
    m_invocationState.trigger.clear();
    m_invocationState.failureMessage.clear();
    m_invocationState.remainingInDegree.clear();
    m_invocationState.nodeDeclarationOrder.clear();
    m_invocationState.readyQueue.clear();
    m_invocationState.completedNodeIds.clear();
    m_invocationState.nodeExecutionResults.clear();
    m_invocationState.branches.clear();
    m_invocationState.nodeBranchIds.clear();
    m_invocationState.nodeResults.clear();
    m_invocationState.pendingByRequestId.clear();
    m_invocationState.activeNodeIds.clear();
}

bool AgentRuntime::PrepareTextInputContext(AgentContext &context, QString &errorMessage)
{
    QVariant triggerTypeValue;
    const bool hasTriggerType = context.GetValue(CONTEXT_KEY_RUNTIME_TRIGGER_TYPE,
                                                  triggerTypeValue);
    const QString triggerType = hasTriggerType ? triggerTypeValue.toString().trimmed()
                                               : QString();
    const bool isUserTrigger = (triggerType == TRIGGER_TYPE_USER);
    const QString userInput = isUserTrigger ? context.GetUserInput().trimmed() : QString();
    const bool isInputAvailable = isUserTrigger && !userInput.isEmpty();

    if (isUserTrigger && !isInputAvailable)
    {
        errorMessage = QStringLiteral("Agent user-triggered execution has no user input.");
        return false;
    }

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
    context.RemoveValue(AgentContextKeys::NODE_OUTPUT_PROMPT);
    context.RemoveValue(CONTEXT_KEY_OUTPUT_TEXT);
    context.RemoveValue(CONTEXT_KEY_OUTPUT_PENDING);
    context.RemoveValue(CONTEXT_KEY_SEMANTIC_OUTPUT_SOURCE);
    context.RemoveValue(CONTEXT_KEY_SEMANTIC_TEXT_FINAL);
    context.RemoveValue(CONTEXT_KEY_SEMANTIC_TEXT_PROMPT);
    context.RemoveValue(CONTEXT_KEY_SEMANTIC_TEXT_RESPONSE);
    context.RemoveValue(AgentContextKeys::SEMANTIC_PROACTIVE_REASON);
    context.RemoveValue(AgentContextKeys::SEMANTIC_PROACTIVE_SHOULD_SPEAK);
    context.RemoveValue(AgentContextKeys::SEMANTIC_PROACTIVE_TOPIC);
    context.RemoveValue(CONTEXT_KEY_PROMPT_TEXT);
    ClearAsyncPendingState(context);

    if (!isInputAvailable)
    {
        emit LogMessage(QStringLiteral("Agent execution has no user text input."));
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

void AgentRuntime::ClearInvocationInputState(AgentContext &context)
{
    context.RemoveValue(AgentContextKeys::USER_INPUT);
    context.RemoveValue(CONTEXT_KEY_INPUT_AVAILABLE);
    context.RemoveValue(CONTEXT_KEY_NODE_INPUT_PROMPT);
    context.RemoveValue(CONTEXT_KEY_PROMPT_TEXT);
    context.RemoveValue(CONTEXT_KEY_RUNTIME_TRIGGER_TYPE);
    context.RemoveValue(CONTEXT_KEY_SEMANTIC_TEXT_PROMPT);
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

    const bool hasVisionBase64 = context.GetValue(CONTEXT_KEY_SEMANTIC_IMAGE_BASE64,
                                                   visionBase64Value)
                                   || context.GetValue(CONTEXT_KEY_VISION_LATEST_BASE64,
                                                       visionBase64Value);
    const bool hasVisionFrameId = context.GetValue(CONTEXT_KEY_SEMANTIC_VISION_FRAME_ID,
                                                    visionFrameIdValue)
                                    || context.GetValue(CONTEXT_KEY_VISION_LATEST_FRAME_ID,
                                                        visionFrameIdValue);
    const bool hasVisionWidth = context.GetValue(CONTEXT_KEY_SEMANTIC_IMAGE_WIDTH,
                                                  visionWidthValue)
                                || context.GetValue(CONTEXT_KEY_VISION_LATEST_WIDTH,
                                                    visionWidthValue);
    const bool hasVisionHeight = context.GetValue(CONTEXT_KEY_SEMANTIC_IMAGE_HEIGHT,
                                                   visionHeightValue)
                                 || context.GetValue(CONTEXT_KEY_VISION_LATEST_HEIGHT,
                                                     visionHeightValue);
    const bool hasVisionModality = context.GetValue(CONTEXT_KEY_SEMANTIC_IMAGE_MEDIA_TYPE,
                                                     visionModalityValue)
                                   || context.GetValue(CONTEXT_KEY_VISION_LATEST_MODALITY,
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

    if (!context.SetValue(CONTEXT_KEY_SEMANTIC_VISION_STATE,
                          hasInput ? QStringLiteral("ready") : QStringLiteral("waiting")))
    {
        errorMessage = QStringLiteral("Agent vision input node failed to record semantic state.");
        return false;
    }

    if (hasInput
        && (!context.SetValue(CONTEXT_KEY_SEMANTIC_IMAGE_BASE64, visionBase64Value)
            || !context.SetValue(CONTEXT_KEY_SEMANTIC_IMAGE_MEDIA_TYPE,
                                 ResolveVisionMediaType(visionModalityValue.toString()))
            || !context.SetValue(CONTEXT_KEY_SEMANTIC_IMAGE_WIDTH, visionWidthValue)
            || !context.SetValue(CONTEXT_KEY_SEMANTIC_IMAGE_HEIGHT, visionHeightValue)
            || !context.SetValue(CONTEXT_KEY_SEMANTIC_VISION_FRAME_ID,
                                 visionFrameIdValue)))
    {
        errorMessage = QStringLiteral("Agent vision input node failed to sync semantic input.");
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

    QVariant visionStateValue;
    QVariant inputAvailableValue;

    if (context.GetValue(CONTEXT_KEY_INPUT_AVAILABLE, inputAvailableValue)
        && inputAvailableValue.toBool())
    {
        emit LogMessage(QStringLiteral("Agent Vision LLM node skipped during text input execution."));
        return true;
    }

    const bool hasSemanticState = context.GetValue(CONTEXT_KEY_SEMANTIC_VISION_STATE,
                                                    visionStateValue);
    const bool hasSemanticReadyState = hasSemanticState
                                       && (visionStateValue.toString() == QStringLiteral("ready"));
    QVariant readyValue;
    const bool hasPrivateReadyState = context.GetValue(CONTEXT_KEY_VISION_INPUT_READY,
                                                        readyValue)
                                      && readyValue.toBool();

    if ((hasSemanticState && !hasSemanticReadyState)
        || (!hasSemanticState && !hasPrivateReadyState))
    {
        emit LogMessage(QStringLiteral("Agent Vision LLM node skipped because vision input is not ready."));
        return true;
    }

    QVariant imageValue;
    QVariant modalityValue;

    if (!context.GetValue(CONTEXT_KEY_SEMANTIC_IMAGE_BASE64, imageValue)
        && !context.GetValue(CONTEXT_KEY_VISION_LATEST_BASE64, imageValue))
    {
        errorMessage = QStringLiteral("Agent Vision LLM node image data is missing.");
        return false;
    }

    if (!context.GetValue(CONTEXT_KEY_SEMANTIC_IMAGE_MEDIA_TYPE, modalityValue)
        && !context.GetValue(CONTEXT_KEY_VISION_LATEST_MODALITY, modalityValue))
    {
        errorMessage = QStringLiteral("Agent Vision LLM node modality is missing.");
        return false;
    }

    const QByteArray base64Image = imageValue.toString().trimmed().toLatin1();
    const QString mediaType = ResolveVisionMediaType(modalityValue.toString());

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

    if (!context.SetValue(CONTEXT_KEY_SEMANTIC_VISION_STATE,
                          QStringLiteral("processing")))
    {
        errorMessage = QStringLiteral("Agent Vision LLM node failed to record semantic state.");
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

    if (!context.GetValue(CONTEXT_KEY_NODE_INPUT_PROMPT, promptValue)
        && !context.GetValue(CONTEXT_KEY_SEMANTIC_TEXT_PROMPT, promptValue)
        && !context.GetValue(CONTEXT_KEY_PROMPT_TEXT, promptValue))
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
        && !context.GetValue(CONTEXT_KEY_SEMANTIC_TEXT_RESPONSE, outputValue)
        && !context.GetValue(CONTEXT_KEY_NODE_INPUT_TEXT_RESPONSE, outputValue)
        && !context.GetValue(CONTEXT_KEY_EMOTION_OUTPUT_TEXT, outputValue)
        && !context.GetValue(AgentContextKeys::LLM_LAST_RESPONSE, outputValue))
    {
        QVariant shouldSpeakValue;

        if (context.GetValue(AgentContextKeys::SEMANTIC_PROACTIVE_SHOULD_SPEAK,
                             shouldSpeakValue)
            && !shouldSpeakValue.toBool())
        {
            context.RemoveValue(CONTEXT_KEY_OUTPUT_PENDING);
            emit LogMessage(QStringLiteral("Agent output node completed without proactive speech."));
            return true;
        }

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

    if (!WriteOutputSource(context, errorMessage))
    {
        return false;
    }

    if (!AgentOutputPolicy::RecordVisionSpeech(context, errorMessage))
    {
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

bool AgentRuntime::WriteOutputSource(AgentContext &context, QString &errorMessage)
{
    QVariant triggerTypeValue;
    const bool hasTriggerType = context.GetValue(CONTEXT_KEY_RUNTIME_TRIGGER_TYPE,
                                                  triggerTypeValue);
    const QString triggerType = hasTriggerType ? triggerTypeValue.toString().trimmed()
                                               : QString();
    const QString outputSource = (triggerType == TRIGGER_TYPE_VISION)
                                 ? OUTPUT_SOURCE_VISION_PROACTIVE
                                 : OUTPUT_SOURCE_USER_RESPONSE;

    if (!context.SetValue(CONTEXT_KEY_SEMANTIC_OUTPUT_SOURCE, outputSource))
    {
        errorMessage = QStringLiteral("Agent output node failed to write output source.");
        return false;
    }

    return true;
}

QString AgentRuntime::ReadOutputSource(const AgentContext &context) const
{
    QVariant outputSourceValue;

    if (!context.GetValue(CONTEXT_KEY_SEMANTIC_OUTPUT_SOURCE, outputSourceValue))
    {
        return OUTPUT_SOURCE_USER_RESPONSE;
    }

    const QString outputSource = outputSourceValue.toString().trimmed();

    if (outputSource == OUTPUT_SOURCE_VISION_PROACTIVE)
    {
        return OUTPUT_SOURCE_VISION_PROACTIVE;
    }

    return OUTPUT_SOURCE_USER_RESPONSE;
}

void AgentRuntime::EmitAgentOutputReady(int requestId, const QString &content)
{
    if (requestId <= 0)
    {
        emit LogMessage(QStringLiteral("Agent output emission skipped because request ID is invalid."));
        return;
    }

    const QString normalizedContent = content.trimmed();

    if (normalizedContent.isEmpty())
    {
        emit LogMessage(QStringLiteral("Agent output emission skipped because content is empty."));
        return;
    }

    const QString outputSource = ReadOutputSource(m_context);
    emit AgentOutputReady(requestId, normalizedContent, outputSource);
}

bool AgentRuntime::AppendConversationHistory(AgentContext &context,
                                             const QString &outputText,
                                             QString &errorMessage)
{
    const QString normalizedUserInput = context.GetUserInput().trimmed();
    const QString normalizedOutputText = outputText.trimmed();

    if (normalizedOutputText.isEmpty())
    {
        errorMessage = QStringLiteral("Agent conversation history output is empty.");
        return false;
    }

    QVariant historyValue;
    QStringList conversationHistory;

    if (context.GetValue(CONTEXT_KEY_CONVERSATION_HISTORY, historyValue))
    {
        conversationHistory = historyValue.toStringList();
    }

    if (!normalizedUserInput.isEmpty())
    {
        conversationHistory.append(QStringLiteral("user: %1").arg(normalizedUserInput));
    }

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
}

void AgentRuntime::ResetAsyncExecutionState(AgentContext &context)
{
    context.SetValue(CONTEXT_KEY_LLM_PENDING, false);
    context.SetValue(CONTEXT_KEY_VISION_LLM_PENDING, false);
    ClearAsyncPendingState(context);
    ClearInvocationInputState(context);
    ClearInvocationState();
    context = m_sessionContext.Snapshot();

    if (!m_invocationQueue.IsEmpty())
    {
        QTimer::singleShot(0, this, [this]()
        {
            QString queuedErrorMessage;

            if (!StartNextQueuedInvocation(queuedErrorMessage) && !queuedErrorMessage.isEmpty())
            {
                emit LogMessage(queuedErrorMessage);
            }
        });
    }
}

bool AgentRuntime::SetAsyncPendingState(const _tagAgentDagNode &node,
                                        AgentContext &context,
                                        int requestId,
                                        QString &errorMessage)
{
    const QString nodeId = node.id.trimmed();
    const QString nodeType = node.type.trimmed();

    if (nodeId.isEmpty() || nodeType.isEmpty())
    {
        errorMessage = QStringLiteral("Agent async pending node is invalid.");
        return false;
    }

    if (requestId <= 0)
    {
        errorMessage = QStringLiteral("Agent async pending request ID is invalid.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_RUNTIME_PENDING, true))
    {
        errorMessage = QStringLiteral("Agent failed to record async pending state.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_RUNTIME_PENDING_NODE_ID, nodeId))
    {
        errorMessage = QStringLiteral("Agent failed to record async pending node id.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_RUNTIME_PENDING_NODE_TYPE, nodeType))
    {
        errorMessage = QStringLiteral("Agent failed to record async pending node type.");
        return false;
    }

    if (!context.SetValue(CONTEXT_KEY_RUNTIME_PENDING_REQUEST_ID, requestId))
    {
        errorMessage = QStringLiteral("Agent failed to record async pending request ID.");
        return false;
    }

    return true;
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
    RegisterNodeHandler(NODE_TYPE_USER_INPUT,
                        [this](const _tagAgentDagNode &node,
                               AgentContext &context,
                               QString &errorMessage)
    {
        return ExecutePassThroughNode(node, context, errorMessage);
    });

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
        int pendingRequestId = -1;

        if (!EmotionRewriteNode::Execute(node,
                                         context,
                                         m_llmClient,
                                         pendingRequestId,
                                         errorMessage))
        {
            return false;
        }

        if (pendingRequestId <= 0)
        {
            return true;
        }

        return SetAsyncPendingState(node, context, pendingRequestId, errorMessage);
    });

    RegisterNodeHandler(NODE_TYPE_OUTPUT_FORMAT,
                        [this](const _tagAgentDagNode &node,
                               AgentContext &context,
                               QString &errorMessage)
    {
        return ExecuteOutputFormatNode(node, context, errorMessage);
    });

    RegisterNodeHandler(NODE_TYPE_PROACTIVE_TOPIC,
                        [](const _tagAgentDagNode &node,
                           AgentContext &context,
                           QString &errorMessage)
    {
        return ProactiveTopicNode::Execute(node, context, errorMessage);
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

    m_directRequestIds.insert(requestId);

    emit LogMessage(QStringLiteral("Agent LLM request sent: %1").arg(requestId));

    return true;
}

void AgentRuntime::OnLlmChatCompleted(int requestId, const QString &content)
{
    if (requestId <= 0)
    {
        emit LogMessage(QStringLiteral("Agent LLM response ignored because request ID is invalid."));
        return;
    }

    if (m_directRequestIds.remove(requestId) > 0)
    {
        if (content.trimmed().isEmpty())
        {
            emit LlmRequestFailed(requestId, QStringLiteral("Agent LLM response is empty."), 0);
            return;
        }

        m_context.SetValue(AgentContextKeys::LLM_LAST_RESPONSE, content);
        m_context.SetValue(CONTEXT_KEY_OUTPUT_TEXT, content);
        m_context.SetValue(CONTEXT_KEY_SEMANTIC_TEXT_FINAL, content);
        m_context.SetValue(CONTEXT_KEY_NODE_OUTPUT_TEXT_FINAL, content);
        m_context.SetValue(CONTEXT_KEY_LLM_PENDING, false);
        ClearAsyncPendingState(m_context);
        ClearInvocationInputState(m_context);
        emit LlmResponseReceived(requestId, content);
        EmitAgentOutputReady(requestId, content);
        return;
    }

    const QString pendingKey = BuildPendingRequestKey(ASYNC_CLIENT_TEXT, requestId);

    if (pendingKey.isEmpty() || !m_invocationState.pendingByRequestId.contains(pendingKey))
    {
        emit LogMessage(QStringLiteral("Agent LLM response ignored because request ID is unknown."));
        return;
    }

    const _tagInvocationState::_tagPendingRequest pendingRequest =
        m_invocationState.pendingByRequestId.value(pendingKey);
    AgentContext callbackContext = pendingRequest.context.Snapshot();
    QString errorMessage;

    if (content.trimmed().isEmpty())
    {
        ResetAsyncExecutionState(m_context);
        emit LlmRequestFailed(requestId, QStringLiteral("Agent LLM response is empty."), 0);
        return;
    }

    if (pendingRequest.nodeType == NODE_TYPE_EMOTION_REWRITE)
    {
        if (!EmotionRewriteNode::Complete(requestId, content, callbackContext, errorMessage))
        {
            ResetAsyncExecutionState(m_context);
            emit LlmRequestFailed(requestId, errorMessage, 0);
            return;
        }

        QVariant emotionOutputValue;

        if (callbackContext.GetValue(CONTEXT_KEY_EMOTION_OUTPUT_TEXT, emotionOutputValue))
        {
            callbackContext.SetValue(CONTEXT_KEY_SEMANTIC_TEXT_RESPONSE, emotionOutputValue);
            callbackContext.SetValue(CONTEXT_KEY_NODE_OUTPUT_TEXT_RESPONSE, emotionOutputValue);
        }
    }
    else if (!callbackContext.SetValue(AgentContextKeys::LLM_LAST_RESPONSE, content)
             || !callbackContext.SetValue(CONTEXT_KEY_SEMANTIC_TEXT_RESPONSE, content)
             || !callbackContext.SetValue(CONTEXT_KEY_NODE_OUTPUT_TEXT_RESPONSE, content))
    {
        ResetAsyncExecutionState(m_context);
        emit LlmRequestFailed(requestId, QStringLiteral("Agent failed to store LLM response."), 0);
        return;
    }

    callbackContext.SetValue(CONTEXT_KEY_LLM_PENDING, false);
    ClearAsyncPendingState(callbackContext);

    if (!ResumePendingNode(pendingKey, requestId, callbackContext, errorMessage))
    {
        ResetAsyncExecutionState(m_context);
        emit LlmRequestFailed(requestId, errorMessage, 0);
        return;
    }

    QVariant outputValue;

    if (!m_invocationState.isActive
        && m_context.GetValue(CONTEXT_KEY_OUTPUT_TEXT, outputValue))
    {
        const QString outputText = outputValue.toString();
        emit LlmResponseReceived(requestId, outputText);
        EmitAgentOutputReady(requestId, outputText);
    }
}

void AgentRuntime::OnLlmChatFailed(int requestId, const QString &message, int statusCode)
{
    if (m_directRequestIds.remove(requestId) > 0)
    {
        emit LlmRequestFailed(requestId, message, statusCode);
        return;
    }

    const QString pendingKey = BuildPendingRequestKey(ASYNC_CLIENT_TEXT, requestId);

    if (pendingKey.isEmpty() || !m_invocationState.pendingByRequestId.contains(pendingKey))
    {
        emit LogMessage(QStringLiteral("Agent LLM failure ignored because request ID is unknown."));
        return;
    }

    ResetAsyncExecutionState(m_context);
    emit LlmRequestFailed(requestId, message, statusCode);
}

void AgentRuntime::OnVisionAnalysisCompleted(int requestId, const QString &content)
{
    const QString pendingKey = BuildPendingRequestKey(ASYNC_CLIENT_VISION, requestId);

    if (pendingKey.isEmpty() || !m_invocationState.pendingByRequestId.contains(pendingKey))
    {
        emit LogMessage(QStringLiteral(
                            "Agent Vision LLM response ignored because pending state does not match."));
        return;
    }

    const QString normalizedContent = content.trimmed();

    if (normalizedContent.isEmpty())
    {
        ResetAsyncExecutionState(m_context);
        m_context.SetValue(CONTEXT_KEY_SEMANTIC_VISION_STATE, QStringLiteral("error"));
        emit LogMessage(QStringLiteral("Agent Vision LLM response is empty."));
        return;
    }

    const _tagInvocationState::_tagPendingRequest pendingRequest =
        m_invocationState.pendingByRequestId.value(pendingKey);

    if (pendingRequest.nodeType != NODE_TYPE_VISION_LLM)
    {
        emit LogMessage(QStringLiteral(
                            "Agent Vision LLM response ignored because pending node type does not match."));
        return;
    }

    AgentContext callbackContext = pendingRequest.context.Snapshot();

    if (!callbackContext.SetValue(CONTEXT_KEY_VISION_ANALYSIS, normalizedContent))
    {
        ResetAsyncExecutionState(m_context);
        m_context.SetValue(CONTEXT_KEY_SEMANTIC_VISION_STATE, QStringLiteral("error"));
        emit LogMessage(QStringLiteral("Agent failed to record Vision LLM analysis."));
        return;
    }

    if (!callbackContext.SetValue(CONTEXT_KEY_SEMANTIC_VISION_SUMMARY, normalizedContent))
    {
        ResetAsyncExecutionState(m_context);
        m_context.SetValue(CONTEXT_KEY_SEMANTIC_VISION_STATE, QStringLiteral("error"));
        emit LogMessage(QStringLiteral("Agent failed to record semantic vision summary."));
        return;
    }

    if (!callbackContext.SetValue(CONTEXT_KEY_SEMANTIC_VISION_STATE,
                                  QStringLiteral("analyzed")))
    {
        ResetAsyncExecutionState(m_context);
        m_context.SetValue(CONTEXT_KEY_SEMANTIC_VISION_STATE, QStringLiteral("error"));
        emit LogMessage(QStringLiteral("Agent failed to record completed semantic vision state."));
        return;
    }

    if (!callbackContext.SetValue(CONTEXT_KEY_VISION_LLM_PENDING, false))
    {
        ResetAsyncExecutionState(m_context);
        m_context.SetValue(CONTEXT_KEY_SEMANTIC_VISION_STATE, QStringLiteral("error"));
        emit LogMessage(QStringLiteral("Agent failed to clear Vision LLM pending state."));
        return;
    }

    ClearAsyncPendingState(callbackContext);
    emit LogMessage(QStringLiteral("Agent Vision LLM analysis completed."));

    QString errorMessage;

    if (!ResumePendingNode(pendingKey, requestId, callbackContext, errorMessage))
    {
        ResetAsyncExecutionState(m_context);
        emit LogMessage(errorMessage);
    }
}

void AgentRuntime::OnVisionAnalysisFailed(int requestId, const QString &message, int statusCode)
{
    (void)statusCode;

    const QString pendingKey = BuildPendingRequestKey(ASYNC_CLIENT_VISION, requestId);

    if (pendingKey.isEmpty() || !m_invocationState.pendingByRequestId.contains(pendingKey))
    {
        emit LogMessage(QStringLiteral(
                            "Agent Vision LLM failure ignored because request ID does not match pending node."));
        return;
    }

    const QString normalizedMessage = message.trimmed();
    const QString outputMessage = normalizedMessage.isEmpty()
                                  ? QStringLiteral("Agent Vision LLM request failed.")
                                  : normalizedMessage;

    ResetAsyncExecutionState(m_context);
    m_context.SetValue(CONTEXT_KEY_SEMANTIC_VISION_STATE, QStringLiteral("error"));

    emit LogMessage(QStringLiteral("Agent Vision LLM request failed: %1 %2").arg(
                        requestId).arg(outputMessage));
}

} // namespace vpet
