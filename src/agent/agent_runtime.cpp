#include "vpet/agent/agent_runtime.h"
#include "vpet/agent/agent_context_keys.h"
#include "vpet/agent/emotion_rewrite_node.h"
#include "vpet/agent/proactive_topic_node.h"
#include "vpet/agent/web_research_node.h"
#include "vpet/agent/agent_output_policy.h"
#include "vpet/llm/llm_client.h"
#include "vpet/llm/vision_llm_client.h"
#include "vpet/web/web_research_engine.h"

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
const QString WEB_SEARCH_CONFIG_FILE_NAME = QStringLiteral("web_search_config.json");
const QString &NODE_TYPE_VISION_INPUT = AgentContextKeys::NODE_TYPE_VISION_INPUT;
const QString &NODE_TYPE_VISION_LLM = AgentContextKeys::NODE_TYPE_VISION_LLM;
const QString &NODE_TYPE_LLM_CHAT = AgentContextKeys::NODE_TYPE_LLM_CHAT;
const QString &NODE_TYPE_EMOTION_REWRITE = AgentContextKeys::NODE_TYPE_EMOTION_REWRITE;
const QString &NODE_TYPE_OUTPUT_FORMAT = AgentContextKeys::NODE_TYPE_OUTPUT_FORMAT;
const QString &NODE_TYPE_PROACTIVE_TOPIC = AgentContextKeys::NODE_TYPE_PROACTIVE_TOPIC;
const QString &NODE_TYPE_USER_INPUT = AgentContextKeys::NODE_TYPE_USER_INPUT;
const QString &NODE_TYPE_WEB_RESEARCH = AgentContextKeys::NODE_TYPE_WEB_RESEARCH;
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
constexpr double LLM_TEMPERATURE_MIN = 0.0;
constexpr double LLM_TEMPERATURE_MAX = 2.0;
constexpr double LLM_TOP_P_MIN = 0.0;
constexpr double LLM_TOP_P_MAX = 1.0;
constexpr double LLM_FREQUENCY_PENALTY_MIN = -2.0;
constexpr double LLM_FREQUENCY_PENALTY_MAX = 2.0;
constexpr double LLM_PRESENCE_PENALTY_MIN = -2.0;
constexpr double LLM_PRESENCE_PENALTY_MAX = 2.0;
constexpr int LLM_MAX_TOKENS_MIN = 1;
constexpr int LLM_MAX_TOKENS_MAX = 32768;
const QString TRIGGER_TYPE_USER = QStringLiteral("user");
const QString TRIGGER_TYPE_VISION = QStringLiteral("vision");
const QString ASYNC_CLIENT_TEXT = QStringLiteral("text");
const QString ASYNC_CLIENT_VISION = QStringLiteral("vision");
const QString ASYNC_CLIENT_WEB = QStringLiteral("web");
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
    : AgentRuntime(nullptr, parent)
{
}

AgentRuntime::AgentRuntime(WebResearchEngine *webResearchEngine, QObject *parent)
    : QObject(parent)
    , m_context()
    , m_sessionContext()
    , m_llmClient(new LlmClient(this))
    , m_visionLlmClient(new VisionLlmClient(this))
    , m_webResearchEngine(webResearchEngine != nullptr
                              ? webResearchEngine
                              : new WebResearchEngine(nullptr, this))
    , m_nodeRegistry()
    , m_graphExecutor()
    , m_asyncBridge()
    , m_invocationQueue()
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
    connect(m_webResearchEngine, &WebResearchEngine::Completed,
            this, &AgentRuntime::OnWebResearchCompleted);
    connect(m_webResearchEngine, &WebResearchEngine::Failed,
            this, &AgentRuntime::OnWebResearchFailed);

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

    if (m_graphExecutor.IsActive() || HasPendingAsyncRequest() || !m_invocationQueue.IsEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime cannot load a DAG during an active invocation.");
        return false;
    }

    if (!m_graphExecutor.Load(configPath, errorMessage))
    {
        m_isLoaded = false;
        return false;
    }

    m_isLoaded = true;

    qDebug() << "[Agent] DAG loaded:" << configPath;
    qDebug() << "[Agent] Topological order:" << m_graphExecutor.GetExecutionOrder();

    return true;
}

bool AgentRuntime::Execute(QString &errorMessage)
{
    if (!m_isLoaded)
    {
        errorMessage = QStringLiteral("Agent runtime is not loaded.");
        return false;
    }

    if (m_graphExecutor.GetExecutionOrder().isEmpty())
    {
        errorMessage = QStringLiteral("Agent runtime execution order is empty.");
        return false;
    }

    if (m_graphExecutor.IsActive() || HasPendingAsyncRequest() || !m_invocationQueue.IsEmpty())
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

    if (!m_graphExecutor.BeginInvocation(m_context,
                                         m_asyncBridge.HasPending(),
                                         errorMessage))
    {
        return false;
    }

    return m_graphExecutor.PumpReadyQueue(true,
                                          m_context,
                                          m_sessionContext,
                                          BuildGraphCallbacks(),
                                          errorMessage);
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
        && (m_graphExecutor.IsActive()
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

bool AgentRuntime::LoadWebSearchConfig(const QString &configPath, QString &errorMessage)
{
    if (configPath.trimmed().isEmpty() || (m_webResearchEngine == nullptr))
    {
        errorMessage = QStringLiteral("Agent web search config input is invalid.");
        return false;
    }

    if (!m_webResearchEngine->LoadClientConfig(configPath, errorMessage))
    {
        return false;
    }

    emit LogMessage(QStringLiteral("Agent web search config loaded: %1").arg(configPath));
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

    if (m_graphExecutor.IsActive() || HasPendingAsyncRequest() || !m_invocationQueue.IsEmpty())
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

bool AgentRuntime::LoadDefaultWebSearchConfig(QString &errorMessage)
{
    const QString configPath = FindDefaultWebSearchConfigPath();

    if (configPath.isEmpty())
    {
        errorMessage = QStringLiteral("web_search_config.json not found.");
        return false;
    }

    return LoadWebSearchConfig(configPath, errorMessage);
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
    return m_asyncBridge.HasRequests();
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
    return m_graphExecutor.GetExecutionOrder();
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
    return m_nodeRegistry.Register(nodeType, handler);
}

bool AgentRuntime::ExecuteNode(const _tagAgentDagNode &node,
                               AgentContext &context,
                               QString &errorMessage)
{
    return m_nodeRegistry.Execute(node, context, errorMessage);
}

bool AgentRuntime::EnqueueInvocation(const AgentContext &context)
{
    return m_invocationQueue.Enqueue(context, m_sessionContext);
}

bool AgentRuntime::StartNextQueuedInvocation(QString &errorMessage)
{
    if (m_graphExecutor.IsActive() || m_invocationQueue.IsEmpty())
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

    if (!m_graphExecutor.BeginInvocation(m_context,
                                         m_asyncBridge.HasPending(),
                                         errorMessage))
    {
        m_context = m_sessionContext.Snapshot();
        return false;
    }

    if (!m_graphExecutor.PumpReadyQueue(true,
                                        m_context,
                                        m_sessionContext,
                                        BuildGraphCallbacks(),
                                        errorMessage))
    {
        return false;
    }

    return true;
}

bool AgentRuntime::RegisterPendingNode(const _tagAgentDagNode &node,
                                       const AgentContext &context,
                                       quint64 invocationId,
                                       const QString &branchId,
                                       QString &errorMessage)
{
    QVariant requestIdValue;
    const QString nodeId = node.id.trimmed();
    const QString nodeType = node.type.trimmed();

    if (!m_graphExecutor.IsActiveInvocation(invocationId)
        || nodeId.isEmpty() || nodeType.isEmpty() || branchId.trimmed().isEmpty()
        || !context.GetValue(CONTEXT_KEY_RUNTIME_PENDING_REQUEST_ID, requestIdValue))
    {
        errorMessage = QStringLiteral("Agent runtime pending node state is invalid.");
        return false;
    }

    const int requestId = requestIdValue.toInt();
    QString clientType = ASYNC_CLIENT_TEXT;

    if (nodeType == NODE_TYPE_VISION_LLM)
    {
        clientType = ASYNC_CLIENT_VISION;
    }
    else if (nodeType == NODE_TYPE_WEB_RESEARCH)
    {
        clientType = ASYNC_CLIENT_WEB;
    }
    const QString pendingKey = m_asyncBridge.BuildRequestKey(clientType, requestId);
    const int timeoutMs = node.config.value(QStringLiteral("async_timeout_ms"))
                              .toInt(DEFAULT_ASYNC_TIMEOUT_MS);

    if ((requestId <= 0) || branchId.isEmpty() || pendingKey.isEmpty()
        || m_asyncBridge.ContainsPending(pendingKey)
        || (timeoutMs <= 0) || (timeoutMs > MAX_ASYNC_TIMEOUT_MS))
    {
        errorMessage = QStringLiteral("Agent runtime pending request is invalid, duplicated, or has an invalid timeout: %1")
                           .arg(requestId);
        return false;
    }

    AgentAsyncBridge::_tagPendingRequest pendingRequest;
    pendingRequest.requestId = requestId;
    pendingRequest.clientType = clientType;
    pendingRequest.invocationId = invocationId;
    pendingRequest.nodeId = nodeId;
    pendingRequest.branchId = branchId;
    pendingRequest.nodeType = nodeType;
    pendingRequest.context = context.Snapshot();
    if (!m_asyncBridge.AddPending(pendingKey, pendingRequest))
    {
        errorMessage = QStringLiteral("Agent runtime pending request is invalid, duplicated, or has an invalid timeout: %1")
                           .arg(requestId);
        return false;
    }

    QTimer::singleShot(timeoutMs, this, [this, pendingKey, requestId, invocationId]()
    {
        HandlePendingRequestTimeout(pendingKey, requestId, invocationId);
    });

    return true;
}

AgentGraphExecutor::_tagCallbacks AgentRuntime::BuildGraphCallbacks()
{
    AgentGraphExecutor::_tagCallbacks callbacks;
    callbacks.prepareInput = [this](AgentContext &context, QString &errorMessage)
    {
        return PrepareTextInputContext(context, errorMessage);
    };
    callbacks.executeNode = [this](const _tagAgentDagNode &node,
                                   AgentContext &context,
                                   QString &errorMessage)
    {
        return ExecuteNode(node, context, errorMessage);
    };
    callbacks.registerPending = [this](const _tagAgentDagNode &node,
                                       const AgentContext &context,
                                       quint64 invocationId,
                                       const QString &branchId,
                                       QString &errorMessage)
    {
        const bool registered = RegisterPendingNode(node,
                                                    context,
                                                    invocationId,
                                                    branchId,
                                                    errorMessage);

        if (registered)
        {
            qDebug() << "[Agent] Node is waiting for async response:" << node.id.trimmed();
        }

        return registered;
    };
    callbacks.hasPending = [this]()
    {
        return m_asyncBridge.HasPending();
    };
    callbacks.clearInput = [this](AgentContext &context)
    {
        ClearInvocationInputState(context);
    };
    callbacks.resetAfterFailure = [this](AgentContext &context)
    {
        ResetAsyncExecutionState(context);
    };
    callbacks.invocationCompleted = [this](AgentContext &context)
    {
        // 唯一可靠的输出点：无论图以 sync 结束还是 async 节点后收尾，
        // 都在 invocationCompleted 发射，避免自定义 DAG 静默丢回复。
        QVariant outputValue;

        if (context.GetValue(CONTEXT_KEY_OUTPUT_TEXT, outputValue))
        {
            const QString outputText = outputValue.toString().trimmed();

            if (!outputText.isEmpty())
            {
                int requestId = 0;
                QVariant requestIdValue;

                if (context.GetValue(CONTEXT_KEY_LLM_LAST_REQUEST_ID, requestIdValue)
                    && (requestIdValue.toInt() > 0))
                {
                    requestId = requestIdValue.toInt();
                }
                else if (context.GetValue(CONTEXT_KEY_RUNTIME_PENDING_REQUEST_ID, requestIdValue)
                         && (requestIdValue.toInt() > 0))
                {
                    requestId = requestIdValue.toInt();
                }
                else if (context.GetValue(CONTEXT_KEY_VISION_LLM_LAST_REQUEST_ID, requestIdValue)
                         && (requestIdValue.toInt() > 0))
                {
                    requestId = requestIdValue.toInt();
                }
                else
                {
                    // 纯同步图可能没有任何 LLM requestId；用 invocation 序号占位，
                    // 保证 UI 仍能收到 AgentOutputReady。
                    requestId = static_cast<int>(m_graphExecutor.GetLastCompletedInvocationId());

                    if (requestId <= 0)
                    {
                        requestId = 1;
                    }
                }

                emit LlmResponseReceived(requestId, outputText);
                EmitAgentOutputReady(requestId, outputText, context);
            }
        }

        if (!m_invocationQueue.IsEmpty())
        {
            QTimer::singleShot(0, this, [this]()
            {
                QString queuedErrorMessage;

                if (!StartNextQueuedInvocation(queuedErrorMessage)
                    && !queuedErrorMessage.isEmpty())
                {
                    emit LogMessage(queuedErrorMessage);
                }
            });
        }

        qDebug() << "[Agent] Ready queue execution finished.";
    };
    return callbacks;
}

QString AgentRuntime::BuildPendingRequestKey(const QString &clientType, int requestId) const
{
    return m_asyncBridge.BuildRequestKey(clientType, requestId);
}

bool AgentRuntime::ResumePendingNode(const QString &pendingKey,
                                     int requestId,
                                     const AgentContext &context,
                                     QString &errorMessage)
{
    if (pendingKey.trimmed().isEmpty() || (requestId <= 0) || !m_graphExecutor.IsActive()
        || !m_asyncBridge.ContainsPending(pendingKey))
    {
        errorMessage = QStringLiteral("Agent runtime has no matching pending request to resume.");
        return false;
    }

    AgentAsyncBridge::_tagPendingRequest pendingRequest;

    if (!m_asyncBridge.TakePending(pendingKey, pendingRequest))
    {
        errorMessage = QStringLiteral("Agent runtime has no matching pending request to resume.");
        return false;
    }

    if (!m_graphExecutor.IsActiveInvocation(pendingRequest.invocationId))
    {
        errorMessage = QStringLiteral("Agent runtime pending request belongs to an old invocation.");
        return false;
    }

    return m_graphExecutor.ResumePendingNode(pendingRequest.nodeId,
                                             pendingRequest.invocationId,
                                             context,
                                             m_context,
                                             m_sessionContext,
                                             BuildGraphCallbacks(),
                                             errorMessage);
}

void AgentRuntime::HandlePendingRequestTimeout(const QString &pendingKey,
                                               int requestId,
                                               quint64 invocationId)
{
    if (pendingKey.trimmed().isEmpty() || (requestId <= 0) || (invocationId == 0)
        || !m_graphExecutor.IsActiveInvocation(invocationId)
        || !m_asyncBridge.ContainsPending(pendingKey))
    {
        return;
    }

    ResetAsyncExecutionState(m_context);
    const QString message = QStringLiteral("Agent async request timed out.");
    emit LogMessage(QStringLiteral("%1 Request ID: %2").arg(message).arg(requestId));
    emit LlmRequestFailed(requestId, message, 0);
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
    context.RemoveValue(AgentContextKeys::SEMANTIC_WEB_RESEARCH_NEED_SEARCH);
    context.RemoveValue(AgentContextKeys::SEMANTIC_WEB_RESEARCH_PLAN);
    context.RemoveValue(AgentContextKeys::SEMANTIC_WEB_RESEARCH_QUERIES);
    context.RemoveValue(AgentContextKeys::SEMANTIC_WEB_RESEARCH_EVIDENCE);
    context.RemoveValue(AgentContextKeys::SEMANTIC_WEB_RESEARCH_UNSUPPORTED_CLAIMS);
    context.RemoveValue(AgentContextKeys::SEMANTIC_WEB_RESEARCH_CONFLICTS);
    context.RemoveValue(AgentContextKeys::SEMANTIC_WEB_RESEARCH_STATUS);
    context.RemoveValue(AgentContextKeys::SEMANTIC_WEB_RESEARCH_CITATIONS);
    context.RemoveValue(AgentContextKeys::SEMANTIC_WEB_RESEARCH_ROUND_COUNT);
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

bool AgentRuntime::ParseLlmRequestOptions(const _tagAgentDagNode &node,
                                          _tagLlmRequestOptions &options,
                                          QString &errorMessage)
{
    const QJsonObject configObject = node.config;

    if (configObject.isEmpty())
    {
        return true;
    }

    const QJsonValue temperatureValue = configObject.value(QStringLiteral("temperature"));

    if (temperatureValue.isUndefined() == false)
    {
        if (!temperatureValue.isDouble())
        {
            errorMessage = QStringLiteral("Agent LLM node temperature is not a number.");
            return false;
        }

        const double temperature = temperatureValue.toDouble();

        if ((temperature < LLM_TEMPERATURE_MIN) || (temperature > LLM_TEMPERATURE_MAX))
        {
            errorMessage = QStringLiteral("Agent LLM node temperature is outside the allowed range.");
            return false;
        }

        options.temperature = temperature;
    }

    const QJsonValue topPValue = configObject.value(QStringLiteral("top_p"));

    if (topPValue.isUndefined() == false)
    {
        if (!topPValue.isDouble())
        {
            errorMessage = QStringLiteral("Agent LLM node top_p is not a number.");
            return false;
        }

        const double topP = topPValue.toDouble();

        if ((topP < LLM_TOP_P_MIN) || (topP > LLM_TOP_P_MAX))
        {
            errorMessage = QStringLiteral("Agent LLM node top_p is outside the allowed range.");
            return false;
        }

        options.topP = topP;
    }

    const QJsonValue frequencyPenaltyValue = configObject.value(QStringLiteral("frequency_penalty"));

    if (frequencyPenaltyValue.isUndefined() == false)
    {
        if (!frequencyPenaltyValue.isDouble())
        {
            errorMessage = QStringLiteral("Agent LLM node frequency_penalty is not a number.");
            return false;
        }

        const double frequencyPenalty = frequencyPenaltyValue.toDouble();

        if ((frequencyPenalty < LLM_FREQUENCY_PENALTY_MIN)
            || (frequencyPenalty > LLM_FREQUENCY_PENALTY_MAX))
        {
            errorMessage = QStringLiteral("Agent LLM node frequency_penalty is outside the allowed range.");
            return false;
        }

        options.frequencyPenalty = frequencyPenalty;
    }

    const QJsonValue presencePenaltyValue = configObject.value(QStringLiteral("presence_penalty"));

    if (presencePenaltyValue.isUndefined() == false)
    {
        if (!presencePenaltyValue.isDouble())
        {
            errorMessage = QStringLiteral("Agent LLM node presence_penalty is not a number.");
            return false;
        }

        const double presencePenalty = presencePenaltyValue.toDouble();

        if ((presencePenalty < LLM_PRESENCE_PENALTY_MIN)
            || (presencePenalty > LLM_PRESENCE_PENALTY_MAX))
        {
            errorMessage = QStringLiteral("Agent LLM node presence_penalty is outside the allowed range.");
            return false;
        }

        options.presencePenalty = presencePenalty;
    }

    const QJsonValue maxTokensValue = configObject.value(QStringLiteral("max_tokens"));

    if (maxTokensValue.isUndefined() == false)
    {
        if (!maxTokensValue.isDouble())
        {
            errorMessage = QStringLiteral("Agent LLM node max_tokens is not a number.");
            return false;
        }

        const int maxTokens = maxTokensValue.toInt();

        if ((maxTokens < LLM_MAX_TOKENS_MIN) || (maxTokens > LLM_MAX_TOKENS_MAX))
        {
            errorMessage = QStringLiteral("Agent LLM node max_tokens is outside the allowed range.");
            return false;
        }

        options.maxTokens = maxTokens;
    }

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

    _tagLlmRequestOptions options;

    if (!ParseLlmRequestOptions(node, options, errorMessage))
    {
        return false;
    }

    const int requestId = m_llmClient->SendPrompt(promptText, options);

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

    emit LogMessage(QStringLiteral("Agent LLM node request sent: %1 (temperature=%2, max_tokens=%3)")
                            .arg(requestId)
                            .arg(options.temperature)
                            .arg(options.maxTokens));

    return true;
}

bool AgentRuntime::ExecuteWebResearchNode(const _tagAgentDagNode &node,
                                          AgentContext &context,
                                          QString &errorMessage)
{
    if (m_webResearchEngine == nullptr)
    {
        errorMessage = QStringLiteral("Agent web research engine is not initialized.");
        return false;
    }

    _tagWebResearchRequest request;

    if (!WebResearchNode::BuildRequest(node, context, request, errorMessage))
    {
        return false;
    }

    const int researchId = m_webResearchEngine->Start(request);

    if (researchId <= 0)
    {
        errorMessage = QStringLiteral("Agent web research node failed to start.");
        return false;
    }

    if (!context.SetValue(AgentContextKeys::WEB_RESEARCH_FAILURE_POLICY,
                          request.config.failurePolicy)
        || !context.SetValue(AgentContextKeys::WEB_RESEARCH_LAST_REQUEST_ID, researchId))
    {
        m_webResearchEngine->Cancel();
        errorMessage = QStringLiteral("Agent web research node failed to record request state.");
        return false;
    }

    if (!SetAsyncPendingState(node, context, researchId, errorMessage))
    {
        m_webResearchEngine->Cancel();
        return false;
    }

    emit LogMessage(QStringLiteral("Agent web research node started: %1").arg(researchId));
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
        QVariant triggerTypeValue;

        if (context.GetValue(CONTEXT_KEY_RUNTIME_TRIGGER_TYPE, triggerTypeValue)
            && (triggerTypeValue.toString().trimmed() == TRIGGER_TYPE_VISION))
        {
            return OUTPUT_SOURCE_VISION_PROACTIVE;
        }

        return OUTPUT_SOURCE_USER_RESPONSE;
    }

    const QString outputSource = outputSourceValue.toString().trimmed();

    if (outputSource == OUTPUT_SOURCE_VISION_PROACTIVE)
    {
        return OUTPUT_SOURCE_VISION_PROACTIVE;
    }

    return OUTPUT_SOURCE_USER_RESPONSE;
}

void AgentRuntime::EmitAgentOutputReady(int requestId,
                                        const QString &content,
                                        const AgentContext &context)
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

    const QString outputSource = ReadOutputSource(context);
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
    m_asyncBridge.ClearContextProtocol(context);
}

void AgentRuntime::ResetAsyncExecutionState(AgentContext &context)
{
    const bool shouldCancelWebResearch = (m_webResearchEngine != nullptr)
                                         && m_webResearchEngine->IsBusy();
    context.SetValue(CONTEXT_KEY_LLM_PENDING, false);
    context.SetValue(CONTEXT_KEY_VISION_LLM_PENDING, false);
    ClearAsyncPendingState(context);
    ClearInvocationInputState(context);
    m_graphExecutor.ClearInvocationState();
    m_asyncBridge.ClearPending();

    if (shouldCancelWebResearch)
    {
        m_webResearchEngine->Cancel();
    }

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
    return m_asyncBridge.SetContextProtocol(node.id,
                                            node.type,
                                            requestId,
                                            context,
                                            errorMessage);
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

    RegisterNodeHandler(NODE_TYPE_WEB_RESEARCH,
                        [this](const _tagAgentDagNode &node,
                               AgentContext &context,
                               QString &errorMessage)
    {
        return ExecuteWebResearchNode(node, context, errorMessage);
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

QString AgentRuntime::FindDefaultWebSearchConfigPath() const
{
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QStringList candidatePaths =
    {
        exeDir + QStringLiteral("/") + WEB_SEARCH_CONFIG_FILE_NAME,
        QDir::currentPath() + QStringLiteral("/") + WEB_SEARCH_CONFIG_FILE_NAME,
        exeDir + QStringLiteral("/../") + WEB_SEARCH_CONFIG_FILE_NAME,
        exeDir + QStringLiteral("/../../") + WEB_SEARCH_CONFIG_FILE_NAME
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

    m_asyncBridge.AddDirectRequest(requestId);

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

    if (m_asyncBridge.TakeDirectRequest(requestId))
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
        EmitAgentOutputReady(requestId, content, m_context);
        return;
    }

    const QString pendingKey = BuildPendingRequestKey(ASYNC_CLIENT_TEXT, requestId);

    if (pendingKey.isEmpty() || !m_asyncBridge.ContainsPending(pendingKey))
    {
        emit LogMessage(QStringLiteral("Agent LLM response ignored because request ID is unknown."));
        return;
    }

    AgentAsyncBridge::_tagPendingRequest pendingRequest;

    if (!m_asyncBridge.GetPending(pendingKey, pendingRequest))
    {
        emit LogMessage(QStringLiteral("Agent LLM response ignored because request ID is unknown."));
        return;
    }
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
             || !callbackContext.SetValue(CONTEXT_KEY_LLM_LAST_REQUEST_ID, requestId)
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

    // AgentOutputReady 由 invocationCompleted 统一发射，避免重复或漏发。
}

void AgentRuntime::OnLlmChatFailed(int requestId, const QString &message, int statusCode)
{
    if (m_asyncBridge.TakeDirectRequest(requestId))
    {
        emit LlmRequestFailed(requestId, message, statusCode);
        return;
    }

    const QString pendingKey = BuildPendingRequestKey(ASYNC_CLIENT_TEXT, requestId);

    if (pendingKey.isEmpty() || !m_asyncBridge.ContainsPending(pendingKey))
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

    if (pendingKey.isEmpty() || !m_asyncBridge.ContainsPending(pendingKey))
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

    AgentAsyncBridge::_tagPendingRequest pendingRequest;

    if (!m_asyncBridge.GetPending(pendingKey, pendingRequest))
    {
        emit LogMessage(QStringLiteral(
                            "Agent Vision LLM response ignored because pending state does not match."));
        return;
    }

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

    if (pendingKey.isEmpty() || !m_asyncBridge.ContainsPending(pendingKey))
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

void AgentRuntime::OnWebResearchCompleted(const _tagWebResearchResponse &response)
{
    const int researchId = response.researchId;
    const QString pendingKey = BuildPendingRequestKey(ASYNC_CLIENT_WEB, researchId);

    if ((researchId <= 0) || pendingKey.isEmpty()
        || !m_asyncBridge.ContainsPending(pendingKey))
    {
        emit LogMessage(QStringLiteral("Agent web research completion ignored because request is unknown."));
        return;
    }

    AgentAsyncBridge::_tagPendingRequest pendingRequest;

    if (!m_asyncBridge.GetPending(pendingKey, pendingRequest)
        || (pendingRequest.clientType != ASYNC_CLIENT_WEB)
        || (pendingRequest.nodeType != NODE_TYPE_WEB_RESEARCH)
        || (pendingRequest.requestId != researchId))
    {
        emit LogMessage(QStringLiteral("Agent web research completion ignored because correlation does not match."));
        return;
    }

    AgentContext callbackContext = pendingRequest.context.Snapshot();
    QString errorMessage;

    if (!WebResearchNode::Complete(response, callbackContext, errorMessage))
    {
        ResetAsyncExecutionState(m_context);
        emit LogMessage(errorMessage);
        return;
    }

    ClearAsyncPendingState(callbackContext);

    if (!ResumePendingNode(pendingKey, researchId, callbackContext, errorMessage))
    {
        ResetAsyncExecutionState(m_context);
        emit LogMessage(errorMessage);
    }
}

void AgentRuntime::OnWebResearchFailed(int researchId,
                                       const QString &message,
                                       int statusCode)
{
    (void)statusCode;
    const QString pendingKey = BuildPendingRequestKey(ASYNC_CLIENT_WEB, researchId);

    if ((researchId <= 0) || pendingKey.isEmpty()
        || !m_asyncBridge.ContainsPending(pendingKey))
    {
        emit LogMessage(QStringLiteral("Agent web research failure ignored because request is unknown."));
        return;
    }

    AgentAsyncBridge::_tagPendingRequest pendingRequest;

    if (!m_asyncBridge.GetPending(pendingKey, pendingRequest)
        || (pendingRequest.clientType != ASYNC_CLIENT_WEB)
        || (pendingRequest.nodeType != NODE_TYPE_WEB_RESEARCH))
    {
        emit LogMessage(QStringLiteral("Agent web research failure ignored because correlation does not match."));
        return;
    }

    AgentContext callbackContext = pendingRequest.context.Snapshot();
    QVariant policyValue;
    const QString failurePolicy = callbackContext.GetValue(
                                      AgentContextKeys::WEB_RESEARCH_FAILURE_POLICY,
                                      policyValue)
                                      ? policyValue.toString().trimmed().toLower()
                                      : QStringLiteral("continue");

    if (failurePolicy == QStringLiteral("fail"))
    {
        ResetAsyncExecutionState(m_context);
        emit LogMessage(QStringLiteral("Agent web research failed with fail policy."));
        return;
    }

    QString errorMessage;

    if (!WebResearchNode::CompleteFailure(message, callbackContext, errorMessage))
    {
        ResetAsyncExecutionState(m_context);
        emit LogMessage(errorMessage);
        return;
    }

    ClearAsyncPendingState(callbackContext);

    if (!ResumePendingNode(pendingKey, researchId, callbackContext, errorMessage))
    {
        ResetAsyncExecutionState(m_context);
        emit LogMessage(errorMessage);
    }
}

} // namespace vpet
