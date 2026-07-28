#ifndef VPET_TTS_SERVER_MANAGER_H
#define VPET_TTS_SERVER_MANAGER_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>

namespace vpet
{

/**
 * @brief TTS 服务器生命周期管理
 *
 * 负责启动、监控、停止 GPT-SoVITS Python 进程。
 * 通过轮询 HTTP 健康检查确认服务器就绪后发射信号。
 * 服务器路径通过 tts_config.json 配置。
 */
class TtsServerManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param[in] parent 父对象
     */
    explicit TtsServerManager(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~TtsServerManager() override;

    /**
     * @brief 从 JSON 文件加载配置并启动 TTS 服务器
     *
     * 查找顺序：可执行文件同目录 > 工作目录。
     *
     * @param[in] configPath 配置文件路径，为空时自动查找
     * @return 启动请求已发出返回 true（结果通过信号通知）
     */
    bool Start(const QString &configPath = QString());

    /**
     * @brief 停止 TTS 服务器进程
     */
    void Stop();

    /**
     * @brief 判断服务器是否就绪
     * @return 服务器已响应健康检查返回 true
     */
    bool IsReady() const;

    /**
     * @brief 获取服务器 URL
     * @return 服务器 URL，如 "http://127.0.0.1:9880"
     */
    QString GetServerUrl() const;

    /**
     * @brief 获取 GPT-SoVITS 工作目录绝对路径
     * @return 工作目录路径；未设置时返回空字符串
     */
    QString GetWorkingDirectory() const;

signals:
    /**
     * @brief TTS 服务器就绪信号
     *
     * 当健康检查首次通过时发射。
     */
    void ServerReady();

    /**
     * @brief TTS 服务器启动失败信号
     * @param[in] errorMessage 错误描述
     */
    void ServerStartFailed(const QString &errorMessage);

    /**
     * @brief 启动进度更新信号
     * @param[in] statusText 当前状态描述文本
     */
    void StatusChanged(const QString &statusText);

private slots:
    /**
     * @brief 定时轮询服务器健康状态
     */
    void OnHealthCheckTimer();

    /**
     * @brief 服务器进程发生错误
     * @param[in] error 进程错误类型
     */
    void OnProcessError(QProcess::ProcessError error);

    /**
     * @brief 服务器进程退出
     * @param[in] exitCode 退出码
     * @param[in] exitStatus 退出状态
     */
    void OnProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    /**
     * @brief 解析配置文件，提取服务器启动所需参数
     * @param[in] configPath 配置文件路径
     * @return 解析成功返回 true
     */
    bool LoadServerConfig(const QString &configPath);

    /**
     * @brief 查找配置文件路径
     *
     * 依次查找：传入路径 > exe 同目录 > 工作目录 > exe 上级目录
     *
     * @param[in] configPath 用户传入路径，为空时自动搜索
     * @return 存在的配置文件路径；未找到返回空字符串
     */
    QString FindConfigFile(const QString &configPath) const;

    /**
     * @brief 执行一次健康检查请求
     */
    void PerformHealthCheck();

private:
    QProcess *m_serverProcess;          ///< GPT-SoVITS 进程
    QTimer *m_healthCheckTimer;         ///< 健康检查轮询定时器
    QString m_serverUrl;                ///< TTS 服务器 URL
    QString m_pythonExePath;            ///< Python 解释器路径
    QString m_apiScriptPath;            ///< api_v2.py 脚本路径
    QString m_workingDirectory;         ///< 服务器工作目录
    QString m_apiArgs;                  ///< API 启动参数
    int m_healthCheckCount;             ///< 已执行健康检查次数
    bool m_isReady;                     ///< 服务器是否就绪

    static constexpr int HEALTH_CHECK_INTERVAL_MS = 800;  ///< 健康检查间隔
    static constexpr int HEALTH_CHECK_TIMEOUT_COUNT = 45; ///< 超时前的检查次数（约 36 秒）
    static constexpr int PROCESS_START_DELAY_MS = 500;    ///< 进程启动后首次检查延迟
};

} // namespace vpet

#endif // VPET_TTS_SERVER_MANAGER_H
