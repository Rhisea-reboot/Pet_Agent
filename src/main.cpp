#include "main_window.h"

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

    vpet::MainWindow window;

    if (!window.Initialize(GetAnimationBasePath()))
    {
        QMessageBox::critical(nullptr,
                              QStringLiteral("VPet 启动失败"),
                              QStringLiteral("无法加载动画资源，请检查 Animation/ 目录。"));
        return 1;
    }

    window.show();

    return application.exec();
}
