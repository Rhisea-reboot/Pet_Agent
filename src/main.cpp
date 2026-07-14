#include "main_window.h"
#include "vpet/splash_window.h"
#include "vpet/tts_server_manager.h"

#include <QApplication>
#include <QDir>
#include <QMessageBox>

/**
 * @brief 获取动画资源根目录
 *
 * 优先使用可执行文件所在目录下的 Animation/；调试时回退到项目根目录。
 *
 * @return 动画资源根目录
 */
static QString GetAnimationBasePath()
{
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QString exeAnimationDir = exeDir + QStringLiteral("/Animation");

    if (QDir(exeAnimationDir).exists())
    {
        return exeAnimationDir;
    }

    const QString projectDir = exeDir + QStringLiteral("/../..");
    const QString projectAnimationDir = QDir(projectDir).absolutePath()
                                        + QStringLiteral("/Animation");

    if (QDir(projectAnimationDir).exists())
    {
        return projectAnimationDir;
    }

    return exeAnimationDir;
}

/**
 * @brief 程序入口
 * @param[in] argc 参数个数
 * @param[in] argv 参数列表
 * @return 进程退出码
 */
int main(int argc, char *argv[])
{
    QApplication application(argc, argv);

    // 创建启动画面
    vpet::SplashWindow splashWindow;
    splashWindow.SetStatusText(QStringLiteral("正在初始化..."));
    splashWindow.show();
    application.processEvents();

    // 创建 TTS 服务管理器并自动启动
    vpet::TtsServerManager ttsServerManager;

    // 启动成功/失败后的共享逻辑
    bool ttsServerReady = false;

    QObject::connect(&ttsServerManager,
                     &vpet::TtsServerManager::StatusChanged,
                     &splashWindow,
                     &vpet::SplashWindow::SetStatusText);

    QObject::connect(&ttsServerManager,
                     &vpet::TtsServerManager::ServerReady,
                     &splashWindow,
                     [&]()
    {
        ttsServerReady = true;
    });

    // 启动 TTS 服务（异步）
    ttsServerManager.Start(QString());

    // 等待 TTS 就绪或超时，同时保持 UI 响应
    // TtsServerManager 内部有超时机制（约 36 秒），超时会发射 ServerStartFailed
    QEventLoop waitLoop;

    QObject::connect(&ttsServerManager,
                     &vpet::TtsServerManager::ServerReady,
                     &waitLoop,
                     &QEventLoop::quit);

    QObject::connect(&ttsServerManager,
                     &vpet::TtsServerManager::ServerStartFailed,
                     &waitLoop,
                     &QEventLoop::quit);

    waitLoop.exec();

    // 隐藏启动画面
    splashWindow.hide();

    // 创建并初始化主窗口
    vpet::MainWindow window;

    if (!window.Initialize(GetAnimationBasePath()))
    {
        QMessageBox::critical(nullptr,
                              QStringLiteral("VPet 启动失败"),
                              QStringLiteral("无法加载动画资源，请检查 Animation/ 目录。"));
        return 1;
    }

    // 如果 TTS 就绪，将服务器 URL 传递给主窗口
    if (ttsServerReady)
    {
        // TTS 配置已在 TtsServerManager 中加载，PetController 中的
        // TtsClient 会通过 tts_config.json 自行完成 HTTP 请求配置
    }

    // 将 TtsServerManager 的所有权转移给 window，确保进程生命周期
    ttsServerManager.setParent(&window);

    window.show();

    const int exitCode = application.exec();

    // 退出前停止 TTS 服务
    ttsServerManager.Stop();

    return exitCode;
}
