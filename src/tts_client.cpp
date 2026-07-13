#include "vpet/tts_client.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace vpet
{

namespace
{

constexpr int HTTP_REQUEST_TIMEOUT_MS = 30000; ///< HTTP 请求超时（毫秒）

} // anonymous namespace

TtsClient::TtsClient(QObject *parent)
    : QObject(parent)
    , m_networkManager(nullptr)
    , m_config()
    , m_isConfigured(false)
    , m_pendingOutputPath()
{
    m_networkManager = new QNetworkAccessManager(this);

    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &TtsClient::OnReplyFinished);
}

TtsClient::~TtsClient()
{
    // m_networkManager 由 QObject 父子关系自动销毁
}

bool TtsClient::LoadConfig(const QString &configPath)
{
    // 检查参数有效性
    if (configPath.isEmpty())
    {
        return false;
    }

    QFile file(configPath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    const QJsonDocument doc = QJsonDocument::fromJson(data);

    if (!doc.isObject())
    {
        return false;
    }

    const QJsonObject obj = doc.object();

    m_config.serverUrl = obj.value(QStringLiteral("server_url")).toString(
                             QStringLiteral("http://127.0.0.1:9880"));

    m_config.refAudioPath = obj.value(QStringLiteral("ref_audio_path")).toString();

    m_config.promptText = obj.value(QStringLiteral("prompt_text")).toString();

    m_config.promptLang = obj.value(QStringLiteral("prompt_lang")).toString(
                              QStringLiteral("zh"));

    m_config.textLang = obj.value(QStringLiteral("text_lang")).toString(
                            QStringLiteral("zh"));

    // 验证必要配置项
    if (m_config.refAudioPath.isEmpty())
    {
        m_isConfigured = false;
        return false;
    }

    m_isConfigured = true;
    return true;
}

bool TtsClient::IsConfigured() const
{
    return m_isConfigured;
}

void TtsClient::Synthesize(const QString &text, const QString &outputPath)
{
    // 检查参数有效性
    if (!m_isConfigured)
    {
        emit SynthesisFinished(QString());
        return;
    }

    if (text.isEmpty() || outputPath.isEmpty())
    {
        emit SynthesisFinished(QString());
        return;
    }

    // 确保输出目录存在
    const QFileInfo fileInfo(outputPath);
    const QDir dir = fileInfo.absoluteDir();

    if (!dir.exists())
    {
        dir.mkpath(QStringLiteral("."));
    }

    m_pendingOutputPath = outputPath;

    // 构建请求 JSON
    QJsonObject body;
    body[QStringLiteral("text")] = text;
    body[QStringLiteral("text_lang")] = m_config.textLang;
    body[QStringLiteral("ref_audio_path")] = m_config.refAudioPath;
    body[QStringLiteral("prompt_lang")] = m_config.promptLang;
    body[QStringLiteral("prompt_text")] = m_config.promptText;
    body[QStringLiteral("streaming_mode")] = false;

    const QByteArray bodyData = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkRequest request(QUrl(m_config.serverUrl + QStringLiteral("/tts")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setTransferTimeout(HTTP_REQUEST_TIMEOUT_MS);

    m_networkManager->post(request, bodyData);
}

void TtsClient::OnReplyFinished(QNetworkReply *reply)
{
    // 检查参数有效性
    if (reply == nullptr)
    {
        emit SynthesisFinished(QString());
        return;
    }

    // 确保 reply 资源在函数结束时释放
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError)
    {
        emit SynthesisFinished(QString());
        return;
    }

    const int statusCode = reply->attribute(
                               QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (statusCode != 200)
    {
        emit SynthesisFinished(QString());
        return;
    }

    const QByteArray audioData = reply->readAll();

    if (audioData.isEmpty())
    {
        emit SynthesisFinished(QString());
        return;
    }

    // 写入音频文件
    QFile outputFile(m_pendingOutputPath);

    if (!outputFile.open(QIODevice::WriteOnly))
    {
        emit SynthesisFinished(QString());
        return;
    }

    outputFile.write(audioData);
    outputFile.close();

    const QString resultPath = m_pendingOutputPath;
    m_pendingOutputPath.clear();

    emit SynthesisFinished(resultPath);
}

} // namespace vpet
