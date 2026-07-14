#include "vpet/tts_server_manager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>

namespace vpet
{

namespace
{

constexpr int HTTP_TIMEOUT_MS = 3000; ///< 健康检查单次请求超时

} // anonymous namespace

TtsServerManager::TtsServerManager(QObject *parent)
    : QObject(parent)
    , m_serverProcess(nullptr)
    , m_healthCheckTimer(nullptr)
    , m_serverUrl()
    , m_pythonExePath()
    , m_apiScriptPath()
    , m_workingDirectory()
    , m_apiArgs()
    , m_healthCheckCount(0)
    , m_isReady(false)
{
}

TtsServerManager::~TtsServerManager()
{
    Stop();
}

bool TtsServerManager::Start(const QString &configPath)
{
    qDebug() << "[TTS] TtsServerManager::Start";

    if (m_serverProcess != nullptr)
    {
        qDebug() << "[TTS]   already started";
        return false;
    }

    emit StatusChanged(QStringLiteral("正在查找配置文件..."));

    const QString resolvedPath = FindConfigFile(configPath);
    qDebug() << "[TTS]   FindConfigFile result:" << resolvedPath;

    if (resolvedPath.isEmpty())
    {
        qDebug() << "[TTS]   FAILED - config file not found";
        emit ServerStartFailed(QStringLiteral("未找到 tts_config.json 配置文件"));
        return false;
    }

    if (!LoadServerConfig(resolvedPath))
    {
        qDebug() << "[TTS]   FAILED - LoadServerConfig failed";
        emit ServerStartFailed(QStringLiteral("TTS 配置文件解析失败"));
        return false;
    }

    qDebug() << "[TTS]   pythonExePath:" << m_pythonExePath;
    qDebug() << "[TTS]   apiScriptPath:" << m_apiScriptPath;
    qDebug() << "[TTS]   workingDir:" << m_workingDirectory;
    qDebug() << "[TTS]   apiArgs:" << m_apiArgs;

    // 检查 Python 解释器是否存在
    if (!QFileInfo::exists(m_pythonExePath))
    {
        qDebug() << "[TTS]   FAILED - python.exe not found at:" << m_pythonExePath;
        emit ServerStartFailed(
            QStringLiteral("未找到 Python 解释器: %1").arg(m_pythonExePath));
        return false;
    }

    // 检查 API 脚本是否存在
    if (!QFileInfo::exists(m_apiScriptPath))
    {
        qDebug() << "[TTS]   FAILED - api_v2.py not found at:" << m_apiScriptPath;
        emit ServerStartFailed(
            QStringLiteral("未找到 API 脚本: %1").arg(m_apiScriptPath));
        return false;
    }

    emit StatusChanged(QStringLiteral("正在清理旧 TTS 进程..."));

    // 精准杀掉占用 9880 端口的残留进程（避免误杀其他 python 进程）
    {
        QProcess netstatProcess;
        netstatProcess.start(QStringLiteral("cmd.exe"),
                             QStringList() << QStringLiteral("/c")
                             << QStringLiteral("for /f \"tokens=5\" %a in "
                                "('netstat -ano ^| findstr :9880 ^| findstr LISTENING') "
                                "do taskkill /F /PID %a"));

        netstatProcess.waitForFinished(5000);

        qDebug() << "[TTS]   port 9880 cleanup done, exitCode:"
                 << netstatProcess.exitCode();
    }

    emit StatusChanged(QStringLiteral("正在启动 TTS 服务..."));

    // 启动 GPT-SoVITS 进程
    m_serverProcess = new QProcess(this);

    connect(m_serverProcess, &QProcess::errorOccurred,
            this, &TtsServerManager::OnProcessError);

    QObject::connect(m_serverProcess,
                     QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     this, &TtsServerManager::OnProcessFinished);

    m_serverProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    m_serverProcess->setWorkingDirectory(m_workingDirectory);

    // 设置 PYTHONPATH 确保优先使用本地 tools/ 和 GPT_SoVITS/
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString pythonPath = m_workingDirectory
                               + QStringLiteral(";")
                               + m_workingDirectory
                               + QStringLiteral("/GPT_SoVITS");

    // 将本地路径插入到 PYTHONPATH 最前面
    const QString existingPythonPath = env.value(QStringLiteral("PYTHONPATH"));

    if (!existingPythonPath.isEmpty())
    {
        env.insert(QStringLiteral("PYTHONPATH"),
                   pythonPath + QStringLiteral(";") + existingPythonPath);
    }
    else
    {
        env.insert(QStringLiteral("PYTHONPATH"), pythonPath);
    }

    m_serverProcess->setProcessEnvironment(env);

    const QStringList arguments = m_apiArgs.split(QLatin1Char(' '),
                                                   Qt::SkipEmptyParts);

    qDebug() << "[TTS]   starting process:" << m_pythonExePath << arguments;
    m_serverProcess->start(m_pythonExePath, arguments);

    if (!m_serverProcess->waitForStarted(5000))
    {
        qDebug() << "[TTS]   FAILED - process failed to start";
        emit ServerStartFailed(QStringLiteral("TTS 服务进程启动失败"));
        return false;
    }

    qDebug() << "[TTS]   process started, PID:" << m_serverProcess->processId();

    emit StatusChanged(QStringLiteral("TTS 服务正在加载模型，请稍候..."));

    // 启动健康检查定时器
    m_healthCheckTimer = new QTimer(this);
    m_healthCheckTimer->setInterval(HEALTH_CHECK_INTERVAL_MS);

    connect(m_healthCheckTimer, &QTimer::timeout,
            this, &TtsServerManager::OnHealthCheckTimer);

    m_healthCheckCount = 0;

    // 延迟首次检查，给服务器一点初始化时间
    QTimer::singleShot(PROCESS_START_DELAY_MS, this, [this]()
    {
        if (m_healthCheckTimer != nullptr)
        {
            m_healthCheckTimer->start();
        }
    });

    qDebug() << "[TTS]   health check timer started, interval:" << HEALTH_CHECK_INTERVAL_MS << "ms";
    return true;
}

void TtsServerManager::Stop()
{
    if (m_healthCheckTimer != nullptr)
    {
        m_healthCheckTimer->stop();
    }

    if (m_serverProcess != nullptr)
    {
        if (m_serverProcess->state() != QProcess::NotRunning)
        {
            m_serverProcess->terminate();

            if (!m_serverProcess->waitForFinished(3000))
            {
                m_serverProcess->kill();
                m_serverProcess->waitForFinished(2000);
            }
        }

        delete m_serverProcess;
        m_serverProcess = nullptr;
    }

    m_isReady = false;
}

bool TtsServerManager::IsReady() const
{
    return m_isReady;
}

QString TtsServerManager::GetServerUrl() const
{
    return m_serverUrl;
}

QString TtsServerManager::GetWorkingDirectory() const
{
    return m_workingDirectory;
}

void TtsServerManager::OnHealthCheckTimer()
{
    m_healthCheckCount += 1;

    // 更新进度文本
    const int dots = (m_healthCheckCount % 4);
    QString progressText = QStringLiteral("TTS 服务正在加载模型，请稍候");

    for (int i = 0; i < dots; ++i)
    {
        progressText += QStringLiteral(".");
    }

    emit StatusChanged(progressText);

    // 超时检查
    if (m_healthCheckCount >= HEALTH_CHECK_TIMEOUT_COUNT)
    {
        qDebug() << "[TTS]   health check TIMEOUT after" << m_healthCheckCount << "attempts";
        m_healthCheckTimer->stop();
        emit ServerStartFailed(
            QStringLiteral("TTS 服务启动超时（约 %1 秒），请检查服务是否正常")
                .arg((HEALTH_CHECK_TIMEOUT_COUNT * HEALTH_CHECK_INTERVAL_MS) / 1000));
        return;
    }

    PerformHealthCheck();
}

void TtsServerManager::OnProcessError(QProcess::ProcessError error)
{
    qDebug() << "[TTS] OnProcessError:" << error;

    if (m_healthCheckTimer != nullptr)
    {
        m_healthCheckTimer->stop();
    }

    const QString errorMsg = m_serverProcess->errorString();
    qDebug() << "[TTS]   process error string:" << errorMsg;
    emit ServerStartFailed(
        QStringLiteral("TTS 服务进程错误: %1").arg(errorMsg));
}

void TtsServerManager::OnProcessFinished(int exitCode,
                                          QProcess::ExitStatus exitStatus)
{
    qDebug() << "[TTS] OnProcessFinished, exitCode:" << exitCode << "exitStatus:" << exitStatus;

    // 仅在服务器尚未就绪时报告退出
    if (!m_isReady)
    {
        if (m_healthCheckTimer != nullptr)
        {
            m_healthCheckTimer->stop();
        }

        const QByteArray stderrOutput = m_serverProcess->readAllStandardError();
        qDebug() << "[TTS]   stderr:" << QString::fromUtf8(stderrOutput);

        emit ServerStartFailed(
            QStringLiteral("TTS 服务进程意外退出，退出码: %1").arg(exitCode));
    }
}

bool TtsServerManager::LoadServerConfig(const QString &configPath)
{
    qDebug() << "[TTS] LoadServerConfig, path:" << configPath;

    QFile file(configPath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "[TTS]   FAILED - cannot open file";
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    const QJsonDocument doc = QJsonDocument::fromJson(data);

    if (!doc.isObject())
    {
        qDebug() << "[TTS]   FAILED - not a JSON object";
        return false;
    }

    const QJsonObject obj = doc.object();

    // 服务器 URL
    m_serverUrl = obj.value(QStringLiteral("server_url")).toString(
                      QStringLiteral("http://127.0.0.1:9880"));

    // Python 解释器与脚本路径 — 相对于 GPT-SoVITS 工作目录
    const QString gptSovitsDir = QFileInfo(configPath).absoluteDir()
                                 .absoluteFilePath(QStringLiteral("GPT-SoVITS"));

    qDebug() << "[TTS]   gptSovitsDir:" << gptSovitsDir;

    m_workingDirectory = gptSovitsDir;

    m_pythonExePath = QDir(gptSovitsDir)
                      .absoluteFilePath(QStringLiteral("runtime/python.exe"));

    m_apiScriptPath = QDir(gptSovitsDir)
                      .absoluteFilePath(QStringLiteral("api_v2.py"));

    // API 启动参数
    const QString host = obj.value(QStringLiteral("server_host")).toString(
                             QStringLiteral("127.0.0.1"));

    const int port = obj.value(QStringLiteral("server_port")).toInt(9880);

    const QString configArg = obj.value(QStringLiteral("server_config")).toString(
                                  QStringLiteral("GPT_SoVITS/configs/tts_infer.yaml"));

    m_apiArgs = QStringLiteral("api_v2.py -a %1 -p %2 -c %3")
                    .arg(host)
                    .arg(port)
                    .arg(configArg);

    qDebug() << "[TTS]   serverUrl:" << m_serverUrl;
    qDebug() << "[TTS]   pythonExePath:" << m_pythonExePath;
    qDebug() << "[TTS]   apiScriptPath:" << m_apiScriptPath;
    qDebug() << "[TTS]   apiArgs:" << m_apiArgs;

    return true;
}

QString TtsServerManager::FindConfigFile(const QString &configPath) const
{
    // 用户传入的路径（最高优先级）
    if (!configPath.isEmpty() && QFile::exists(configPath))
    {
        return QFileInfo(configPath).absoluteFilePath();
    }

    const QString exeDir = QCoreApplication::applicationDirPath();

    // 构建候选路径列表
    const QStringList candidatePaths =
    {
        // exe 同目录
        exeDir + QStringLiteral("/tts_config.json"),

        // 工作目录
        QDir::currentPath() + QStringLiteral("/tts_config.json"),

        // exe 上级目录（exe 在 build/ 下，配置在项目根目录）
        exeDir + QStringLiteral("/../tts_config.json"),

        // exe 上两级目录（Qt Creator Debug 模式 exe 在 build/Debug/ 下）
        exeDir + QStringLiteral("/../../tts_config.json"),
    };

    for (const QString &candidate : candidatePaths)
    {
        if (QFile::exists(candidate))
        {
            return QFileInfo(candidate).absoluteFilePath();
        }
    }

    return QString();
}

void TtsServerManager::PerformHealthCheck()
{
    QNetworkAccessManager *networkManager = new QNetworkAccessManager(this);

    QNetworkRequest request(QUrl(m_serverUrl + QStringLiteral("/docs")));
    request.setTransferTimeout(HTTP_TIMEOUT_MS);

    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, networkManager]()
    {
        reply->deleteLater();
        networkManager->deleteLater();

        const int statusCode = reply->attribute(
                                   QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() == QNetworkReply::NoError)
        {
            qDebug() << "[TTS]   health check PASSED, HTTP" << statusCode;

            if (m_healthCheckTimer != nullptr)
            {
                m_healthCheckTimer->stop();
            }

            m_isReady = true;
            emit StatusChanged(QStringLiteral("TTS 服务就绪！"));
            emit ServerReady();
            return;
        }

        // 部分情况下服务器已就绪但 /docs 不可用，尝试 /tts 端点
        // 只要不是 ConnectionRefusedError 就认为在启动中
        qDebug() << "[TTS]   health check#" << m_healthCheckCount
                 << "failed:" << reply->errorString();
    });
}

} // namespace vpet
