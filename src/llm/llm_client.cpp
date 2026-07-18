#include "vpet/llm/llm_client.h"

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QScopeGuard>
#include <QUrl>
#include <QtGlobal>

namespace vpet
{

namespace
{

constexpr int DEFAULT_TIMEOUT_MS = 30000;
constexpr int MIN_TIMEOUT_MS = 1000;
constexpr int MAX_TIMEOUT_MS = 120000;
constexpr int MIN_MAX_TOKENS = 1;
constexpr int MAX_MAX_TOKENS = 128000;
constexpr double MIN_TEMPERATURE = 0.0;
constexpr double MAX_TEMPERATURE = 2.0;
constexpr double MIN_TOP_P = 0.0;
constexpr double MAX_TOP_P = 1.0;
constexpr double MIN_PENALTY = -2.0;
constexpr double MAX_PENALTY = 2.0;

} // anonymous namespace

LlmClient::LlmClient(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_config()
    , m_isConfigured(false)
    , m_nextRequestId(1)
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &LlmClient::OnReplyFinished);
}

LlmClient::~LlmClient()
{
    // m_networkManager 由 QObject 父子关系自动销毁。
}

bool LlmClient::LoadConfig(const QString &configPath)
{
    if (configPath.trimmed().isEmpty())
    {
        emit ChatFailed(-1, QStringLiteral("LLM config path is empty."), 0);
        return false;
    }

    QFile file(configPath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        emit ChatFailed(-1, QStringLiteral("Failed to open LLM config file."), 0);
        return false;
    }

    const QByteArray configData = file.readAll();
    file.close();

    const QJsonDocument document = QJsonDocument::fromJson(configData);

    if (!document.isObject())
    {
        emit ChatFailed(-1, QStringLiteral("LLM config is not a JSON object."), 0);
        return false;
    }

    const QJsonObject object = document.object();
    _tagLlmConfig config;
    config.baseUrl = object.value(QStringLiteral("base_url")).toString(
                         QStringLiteral("https://api.openai.com/v1"));
    config.apiKeyEnvName = object.value(QStringLiteral("api_key_env")).toString(
                               QStringLiteral("OPENAI_API_KEY"));
    config.model = object.value(QStringLiteral("model")).toString(
                       QStringLiteral("gpt-4o-mini"));
    config.timeoutMs = object.value(QStringLiteral("timeout_ms")).toInt(DEFAULT_TIMEOUT_MS);

    return SetConfig(config);
}

bool LlmClient::SetConfig(const _tagLlmConfig &config)
{
    QString errorMessage;
    _tagLlmConfig normalizedConfig;
    const bool isValid = NormalizeConfig(config, normalizedConfig, errorMessage);

    if (!isValid)
    {
        m_isConfigured = false;
        emit ChatFailed(-1, errorMessage, 0);
        return false;
    }

    m_config = normalizedConfig;
    m_isConfigured = true;

    return true;
}

bool LlmClient::IsConfigured() const
{
    return m_isConfigured;
}

int LlmClient::SendChat(const QVector<_tagLlmMessage> &messages,
                        const _tagLlmRequestOptions &options)
{
    if (!m_isConfigured)
    {
        emit ChatFailed(-1, QStringLiteral("LLM client is not configured."), 0);
        return -1;
    }

    if (messages.isEmpty())
    {
        emit ChatFailed(-1, QStringLiteral("LLM messages are empty."), 0);
        return -1;
    }

    QString apiKey;
    QString errorMessage;

    if (!LoadApiKey(apiKey, errorMessage))
    {
        emit ChatFailed(-1, errorMessage, 0);
        return -1;
    }

    QJsonArray messageArray;

    for (const _tagLlmMessage &message : messages)
    {
        if (message.content.trimmed().isEmpty())
        {
            emit ChatFailed(-1, QStringLiteral("LLM message content is empty."), 0);
            return -1;
        }

        QJsonObject messageObject;
        messageObject[QStringLiteral("role")] = RoleToString(message.role);
        messageObject[QStringLiteral("content")] = message.content;
        messageArray.append(messageObject);
    }

    const _tagLlmRequestOptions normalizedOptions = NormalizeOptions(options);
    QJsonObject body;
    body[QStringLiteral("model")] = m_config.model;
    body[QStringLiteral("messages")] = messageArray;
    body[QStringLiteral("temperature")] = normalizedOptions.temperature;
    body[QStringLiteral("top_p")] = normalizedOptions.topP;
    body[QStringLiteral("frequency_penalty")] = normalizedOptions.frequencyPenalty;
    body[QStringLiteral("presence_penalty")] = normalizedOptions.presencePenalty;
    body[QStringLiteral("max_tokens")] = normalizedOptions.maxTokens;
    body[QStringLiteral("stream")] = false;

    const QByteArray bodyData = QJsonDocument(body).toJson(QJsonDocument::Compact);
    const QUrl requestUrl(m_config.baseUrl + QStringLiteral("/chat/completions"));

    if (!requestUrl.isValid())
    {
        emit ChatFailed(-1, QStringLiteral("LLM request URL is invalid."), 0);
        return -1;
    }

    QNetworkRequest request(requestUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey.toUtf8());
    request.setTransferTimeout(m_config.timeoutMs);

    const int requestId = m_nextRequestId;
    m_nextRequestId += 1;

    QNetworkReply *reply = m_networkManager->post(request, bodyData);

    if (reply == nullptr)
    {
        emit ChatFailed(requestId, QStringLiteral("Failed to create LLM HTTP request."), 0);
        return -1;
    }

    reply->setProperty("requestId", requestId);

    return requestId;
}

int LlmClient::SendChat(const QVector<_tagLlmMessage> &messages)
{
    const _tagLlmRequestOptions options;

    return SendChat(messages, options);
}

int LlmClient::SendPrompt(const QString &prompt, const _tagLlmRequestOptions &options)
{
    if (prompt.trimmed().isEmpty())
    {
        emit ChatFailed(-1, QStringLiteral("LLM prompt is empty."), 0);
        return -1;
    }

    _tagLlmMessage message;
    message.role = LLM_MESSAGE_ROLE::USER;
    message.content = prompt;

    QVector<_tagLlmMessage> messages;
    messages.append(message);

    return SendChat(messages, options);
}

int LlmClient::SendPrompt(const QString &prompt)
{
    const _tagLlmRequestOptions options;

    return SendPrompt(prompt, options);
}

void LlmClient::OnReplyFinished(QNetworkReply *reply)
{
    if (reply == nullptr)
    {
        emit ChatFailed(-1, QStringLiteral("LLM reply is null."), 0);
        return;
    }

    const auto replyGuard = qScopeGuard([reply]()
    {
        reply->deleteLater();
    });

    const int requestId = reply->property("requestId").toInt();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray responseData = reply->readAll();

    if (requestId <= 0)
    {
        emit ChatFailed(-1, QStringLiteral("LLM reply does not contain request ID."), statusCode);
        return;
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        const QString message = QStringLiteral("LLM network error: %1").arg(reply->errorString());
        emit ChatFailed(requestId, message, statusCode);
        return;
    }

    if ((statusCode < 200) || (statusCode >= 300))
    {
        const QString message = QStringLiteral("LLM HTTP error: %1").arg(QString::fromUtf8(responseData));
        emit ChatFailed(requestId, message, statusCode);
        return;
    }

    QString content;
    QString errorMessage;

    if (!ExtractAssistantContent(responseData, content, errorMessage))
    {
        emit ChatFailed(requestId, errorMessage, statusCode);
        return;
    }

    emit ChatCompleted(requestId, content);
}

bool LlmClient::NormalizeConfig(const _tagLlmConfig &config,
                                _tagLlmConfig &normalizedConfig,
                                QString &errorMessage)
{
    normalizedConfig = config;
    normalizedConfig.baseUrl = normalizedConfig.baseUrl.trimmed();
    normalizedConfig.apiKeyEnvName = normalizedConfig.apiKeyEnvName.trimmed();
    normalizedConfig.model = normalizedConfig.model.trimmed();

    if (normalizedConfig.baseUrl.isEmpty())
    {
        errorMessage = QStringLiteral("LLM base URL is empty.");
        return false;
    }

    if (normalizedConfig.baseUrl.endsWith(QStringLiteral("/")))
    {
        normalizedConfig.baseUrl.chop(1);
    }

    const QUrl baseUrl(normalizedConfig.baseUrl);

    if (!baseUrl.isValid() || baseUrl.scheme().isEmpty() || baseUrl.host().isEmpty())
    {
        errorMessage = QStringLiteral("LLM base URL is invalid.");
        return false;
    }

    if (normalizedConfig.apiKeyEnvName.isEmpty())
    {
        errorMessage = QStringLiteral("LLM API key environment variable name is empty.");
        return false;
    }

    if (normalizedConfig.model.isEmpty())
    {
        errorMessage = QStringLiteral("LLM model is empty.");
        return false;
    }

    if ((normalizedConfig.timeoutMs < MIN_TIMEOUT_MS)
        || (normalizedConfig.timeoutMs > MAX_TIMEOUT_MS))
    {
        normalizedConfig.timeoutMs = DEFAULT_TIMEOUT_MS;
    }

    return true;
}

_tagLlmRequestOptions LlmClient::NormalizeOptions(const _tagLlmRequestOptions &options)
{
    _tagLlmRequestOptions normalizedOptions = options;
    normalizedOptions.temperature = qBound(MIN_TEMPERATURE,
                                           normalizedOptions.temperature,
                                           MAX_TEMPERATURE);
    normalizedOptions.topP = qBound(MIN_TOP_P, normalizedOptions.topP, MAX_TOP_P);
    normalizedOptions.frequencyPenalty = qBound(MIN_PENALTY,
                                                normalizedOptions.frequencyPenalty,
                                                MAX_PENALTY);
    normalizedOptions.presencePenalty = qBound(MIN_PENALTY,
                                               normalizedOptions.presencePenalty,
                                               MAX_PENALTY);
    normalizedOptions.maxTokens = qBound(MIN_MAX_TOKENS,
                                         normalizedOptions.maxTokens,
                                         MAX_MAX_TOKENS);

    return normalizedOptions;
}

QString LlmClient::RoleToString(LLM_MESSAGE_ROLE role)
{
    switch (role)
    {
    case LLM_MESSAGE_ROLE::SYSTEM:
        return QStringLiteral("system");

    case LLM_MESSAGE_ROLE::USER:
        return QStringLiteral("user");

    case LLM_MESSAGE_ROLE::ASSISTANT:
        return QStringLiteral("assistant");

    case LLM_MESSAGE_ROLE::TOOL:
        return QStringLiteral("tool");
    }

    return QStringLiteral("user");
}

bool LlmClient::ExtractAssistantContent(const QByteArray &responseData,
                                        QString &content,
                                        QString &errorMessage)
{
    if (responseData.isEmpty())
    {
        errorMessage = QStringLiteral("LLM response body is empty.");
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(responseData);

    if (!document.isObject())
    {
        errorMessage = QStringLiteral("LLM response is not a JSON object.");
        return false;
    }

    const QJsonArray choices = document.object().value(QStringLiteral("choices")).toArray();

    if (choices.isEmpty())
    {
        errorMessage = QStringLiteral("LLM response choices are empty.");
        return false;
    }

    const QJsonObject firstChoice = choices.at(0).toObject();
    const QJsonObject message = firstChoice.value(QStringLiteral("message")).toObject();
    content = message.value(QStringLiteral("content")).toString();

    if (content.isEmpty())
    {
        errorMessage = QStringLiteral("LLM response content is empty.");
        return false;
    }

    return true;
}

bool LlmClient::LoadApiKey(QString &apiKey, QString &errorMessage) const
{
    if (m_config.apiKeyEnvName.trimmed().isEmpty())
    {
        errorMessage = QStringLiteral("LLM API key environment variable name is empty.");
        return false;
    }

    const QByteArray envName = m_config.apiKeyEnvName.toUtf8();
    const QByteArray envValue = qgetenv(envName.constData());

    if (envValue.isEmpty())
    {
        errorMessage = QStringLiteral("LLM API key environment variable is not set.");
        return false;
    }

    apiKey = QString::fromUtf8(envValue).trimmed();

    if (apiKey.isEmpty())
    {
        errorMessage = QStringLiteral("LLM API key is empty.");
        return false;
    }

    return true;
}

} // namespace vpet
