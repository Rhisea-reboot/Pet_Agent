#include "vpet/tts_client.h"

#include <QDebug>
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
    qDebug() << "[TTS] TtsClient::LoadConfig, path:" << configPath;

    // 检查参数有效性
    if (configPath.isEmpty())
    {
        qDebug() << "[TTS]   FAILED - empty config path";
        return false;
    }

    QFile file(configPath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "[TTS]   FAILED - cannot open config file:" << configPath;
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    const QJsonDocument doc = QJsonDocument::fromJson(data);

    if (!doc.isObject())
    {
        qDebug() << "[TTS]   FAILED - invalid JSON";
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

    qDebug() << "[TTS]   serverUrl:" << m_config.serverUrl;
    qDebug() << "[TTS]   refAudioPath:" << m_config.refAudioPath;
    qDebug() << "[TTS]   promptText:" << m_config.promptText;

    // 验证必要配置项
    if (m_config.refAudioPath.isEmpty())
    {
        qDebug() << "[TTS]   FAILED - ref_audio_path is empty";
        m_isConfigured = false;
        return false;
    }

    m_isConfigured = true;
    qDebug() << "[TTS]   config loaded successfully";
    return true;
}

bool TtsClient::IsConfigured() const
{
    return m_isConfigured;
}

void TtsClient::Synthesize(const QString &text, const QString &outputPath)
{
    qDebug() << "[TTS] TtsClient::Synthesize";
    qDebug() << "[TTS]   text:" << text;
    qDebug() << "[TTS]   output:" << outputPath;

    // 检查参数有效性
    if (!m_isConfigured)
    {
        qDebug() << "[TTS]   FAILED - not configured";
        emit SynthesisFinished(QString());
        return;
    }

    if (text.isEmpty() || outputPath.isEmpty())
    {
        qDebug() << "[TTS]   FAILED - empty text or outputPath";
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

    // 构建请求 JSON
    QJsonObject body;
    body[QStringLiteral("text")] = text;
    body[QStringLiteral("text_lang")] = m_config.textLang;
    body[QStringLiteral("ref_audio_path")] = m_config.refAudioPath;
    body[QStringLiteral("prompt_lang")] = m_config.promptLang;
    body[QStringLiteral("prompt_text")] = m_config.promptText;
    body[QStringLiteral("streaming_mode")] = false;

    const QByteArray bodyData = QJsonDocument(body).toJson(QJsonDocument::Compact);

    qDebug() << "[TTS]   POST to:" << (m_config.serverUrl + QStringLiteral("/tts"));
    qDebug() << "[TTS]   body:" << QString::fromUtf8(bodyData);

    QNetworkRequest request(QUrl(m_config.serverUrl + QStringLiteral("/tts")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setTransferTimeout(HTTP_REQUEST_TIMEOUT_MS);

    QNetworkReply *reply = m_networkManager->post(request, bodyData);

    // 将输出路径绑定到此次请求的 reply 上，防止并发请求时路径错乱
    reply->setProperty("outputPath", outputPath);

    qDebug() << "[TTS]   request sent, waiting for reply...";
}

void TtsClient::OnReplyFinished(QNetworkReply *reply)
{
    qDebug() << "[TTS] TtsClient::OnReplyFinished";

    // 检查参数有效性
    if (reply == nullptr)
    {
        qDebug() << "[TTS]   FAILED - null reply";
        emit SynthesisFinished(QString());
        return;
    }

    // 从 reply 属性中获取本次请求的输出路径
    const QString outputPath = reply->property("outputPath").toString();

    // 确保 reply 资源在函数结束时释放
    reply->deleteLater();

    if (outputPath.isEmpty())
    {
        qDebug() << "[TTS]   FAILED - no outputPath property on reply";
        emit SynthesisFinished(QString());
        return;
    }

    const int statusCode = reply->attribute(
                               QNetworkRequest::HttpStatusCodeAttribute).toInt();

    qDebug() << "[TTS]   HTTP status:" << statusCode;

    if (reply->error() != QNetworkReply::NoError)
    {
        const QByteArray errorBody = reply->readAll();
        qDebug() << "[TTS]   FAILED - network error:" << reply->errorString();
        qDebug() << "[TTS]   server response body:" << QString::fromUtf8(errorBody);
        return;
    }

    if (statusCode != 200)
    {
        const QByteArray errorBody = reply->readAll();
        qDebug() << "[TTS]   FAILED - HTTP" << statusCode << "error body:" << QString::fromUtf8(errorBody);
        return;
    }

    const QByteArray audioData = reply->readAll();

    qDebug() << "[TTS]   received audio data size:" << audioData.size() << "bytes";

    if (audioData.isEmpty())
    {
        qDebug() << "[TTS]   FAILED - empty audio data";
        return;
    }

    // 写入音频文件
    QFile outputFile(outputPath);

    if (!outputFile.open(QIODevice::WriteOnly))
    {
        qDebug() << "[TTS]   FAILED - cannot write to:" << outputPath;
        emit SynthesisFinished(outputPath);
        return;
    }

    const qint64 bytesWritten = outputFile.write(audioData);
    outputFile.close();

    qDebug() << "[TTS]   wrote" << bytesWritten << "bytes to" << outputPath;
    qDebug() << "[TTS]   synthesis SUCCESS, file:" << outputPath;

    emit SynthesisFinished(outputPath);
}

} // namespace vpet
