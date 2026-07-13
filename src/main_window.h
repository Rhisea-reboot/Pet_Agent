#ifndef VPET_MAIN_WINDOW_H
#define VPET_MAIN_WINDOW_H

#include "vpet/pet_controller.h"

#include <QLabel>
#include <QMouseEvent>
#include <QWidget>

namespace vpet
{

/**
 * @brief 桌宠主窗口
 *
 * 透明无边框置顶窗口，负责渲染当前帧、显示气泡、转发鼠标事件。
 */
class MainWindow : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param[in] parent 父窗口
     */
    explicit MainWindow(QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~MainWindow() override;

    /**
     * @brief 初始化窗口与控制器
     * @param[in] animationBasePath 动画资源根目录
     * @return 初始化成功返回 true
     */
    bool Initialize(const QString &animationBasePath);

protected:
    /**
     * @brief 鼠标按下事件
     * @param[in] event 鼠标事件
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * @brief 鼠标移动事件
     * @param[in] event 鼠标事件
     */
    void mouseMoveEvent(QMouseEvent *event) override;

    /**
     * @brief 鼠标释放事件
     * @param[in] event 鼠标事件
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    /**
     * @brief 帧变化槽
     * @param[in] framePath 新帧路径
     */
    void OnFrameChanged(const QString &framePath);

    /**
     * @brief 气泡变化槽
     * @param[in] visible 是否可见
     * @param[in] text 气泡文本
     */
    void OnBubbleChanged(bool visible, const QString &text);

    /**
     * @brief 位置变化槽
     * @param[in] position 新位置
     */
    void OnPositionChanged(const QPoint &position);

    /**
     * @brief 状态变化槽
     * @param[in] newState 新状态
     */
    void OnStateChanged(PET_STATE newState);

    /**
     * @brief 说话动画开始槽
     *
     * 预留接口：后续可在此触发 XAML 聊天气泡绘制。
     *
     * @param[in] groupName Say 分组名，如 "say_self"
     */
    void OnSayStarted(const QString &groupName);

private:
    /**
     * @brief 根据图片尺寸更新命中区域
     * @param[in] imageSize 图片尺寸
     */
    void UpdateHitRegions(const QSize &imageSize);

    /**
     * @brief 将窗口居中到主屏幕
     */
    void CenterOnScreen();

private:
    PetController *m_controller; ///< 宠物控制器
    QLabel *m_imageLabel;        ///< 帧显示标签
    QLabel *m_bubbleLabel;       ///< 气泡标签
    QSize m_currentImageSize;    ///< 当前图片尺寸
};

} // namespace vpet

#endif // VPET_MAIN_WINDOW_H
