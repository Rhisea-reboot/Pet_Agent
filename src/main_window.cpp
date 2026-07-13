#include "main_window.h"

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>

namespace vpet
{

namespace
{

constexpr int BUBBLE_OFFSET_Y = 10;     ///< 气泡与窗口顶部的偏移
constexpr int TARGET_DISPLAY_WIDTH = 300; ///< 宠物显示宽度，按原图比例缩放

} // anonymous namespace

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , m_controller(nullptr)
    , m_imageLabel(nullptr)
    , m_bubbleLabel(nullptr)
    , m_currentImageSize()
{
    setWindowFlags(Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint
                   | Qt::Tool
                   | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setScaledContents(false);

    m_bubbleLabel = new QLabel(this);
    m_bubbleLabel->setVisible(false);
    m_bubbleLabel->setStyleSheet(
        QStringLiteral("QLabel { background-color: white; color: black; "
                       "border: 1px solid gray; border-radius: 8px; "
                       "padding: 6px 10px; font-size: 12px; }"));
}

MainWindow::~MainWindow()
{
    delete m_controller;
}

bool MainWindow::Initialize(const QString &animationBasePath)
{
    m_controller = new PetController(animationBasePath, this);

    if (!m_controller->Initialize())
    {
        return false;
    }

    const QScreen *screen = QGuiApplication::primaryScreen();

    if (screen != nullptr)
    {
        m_controller->SetScreenBounds(screen->availableGeometry());
    }

    connect(m_controller, &PetController::FrameChanged,
            this, &MainWindow::OnFrameChanged);
    connect(m_controller, &PetController::BubbleChanged,
            this, &MainWindow::OnBubbleChanged);
    connect(m_controller, &PetController::PositionChanged,
            this, &MainWindow::OnPositionChanged);
    connect(m_controller, &PetController::StateChanged,
            this, &MainWindow::OnStateChanged);
    connect(m_controller, &PetController::SayStarted,
            this, &MainWindow::OnSayStarted);

    CenterOnScreen();

    const QString framePath = m_controller->GetCurrentFramePath();

    if (!framePath.isEmpty())
    {
        OnFrameChanged(framePath);
    }

    return true;
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
    {
        QWidget::mousePressEvent(event);
        return;
    }

    if (m_controller != nullptr)
    {
        m_controller->OnMousePress(event->pos());
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_controller != nullptr)
    {
        m_controller->OnMouseMove(event->globalPosition().toPoint());
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
    {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    if (m_controller != nullptr)
    {
        m_controller->OnMouseRelease(event->pos());
    }
}

void MainWindow::OnFrameChanged(const QString &framePath)
{
    const QPixmap originalPixmap(framePath);

    if (originalPixmap.isNull())
    {
        return;
    }

    const bool isFirstFrame = !m_currentImageSize.isValid();
    const QPixmap displayPixmap = originalPixmap.scaledToWidth(
        TARGET_DISPLAY_WIDTH, Qt::SmoothTransformation);

    m_currentImageSize = displayPixmap.size();
    m_imageLabel->setPixmap(displayPixmap);
    m_imageLabel->resize(m_currentImageSize);

    if (m_controller != nullptr)
    {
        m_controller->SetFrameSize(m_currentImageSize);
    }

    UpdateHitRegions(m_currentImageSize);

    resize(m_currentImageSize);
    update();

    if (isFirstFrame)
    {
        CenterOnScreen();
    }
}

void MainWindow::OnBubbleChanged(bool visible, const QString &text)
{
    m_bubbleLabel->setText(text);
    m_bubbleLabel->adjustSize();

    if (visible)
    {
        const int x = (width() - m_bubbleLabel->width()) / 2;
        const int y = -m_bubbleLabel->height() - BUBBLE_OFFSET_Y;
        m_bubbleLabel->move(x, y);
    }

    m_bubbleLabel->setVisible(visible);
}

void MainWindow::OnPositionChanged(const QPoint &position)
{
    move(position);
}

void MainWindow::OnStateChanged(PET_STATE newState)
{
    (void)newState;
}

void MainWindow::UpdateHitRegions(const QSize &imageSize)
{
    if (!imageSize.isValid())
    {
        return;
    }

    const int headBottom = imageSize.height() / 3;
    const int bodyTop = headBottom;

    const QRect headRegion(0, 0, imageSize.width(), headBottom);
    const QRect bodyRegion(0, bodyTop, imageSize.width(), imageSize.height() - bodyTop);
    const QRect dragRegion; // 空：拖拽由移动距离判断，不依赖固定区域

    m_controller->SetHitRegions(headRegion, bodyRegion, dragRegion);
}

void MainWindow::CenterOnScreen()
{
    const QScreen *screen = QGuiApplication::primaryScreen();

    if (screen == nullptr)
    {
        return;
    }

    const QRect screenGeometry = screen->availableGeometry();
    const QPoint center = screenGeometry.center();

    const QSize frameSize = m_currentImageSize.isValid()
                            ? m_currentImageSize
                            : QSize(TARGET_DISPLAY_WIDTH, TARGET_DISPLAY_WIDTH);

    const QPoint position(center.x() - (frameSize.width() / 2),
                          center.y() - (frameSize.height() / 2));

    m_controller->SetPosition(position);
}

void MainWindow::OnSayStarted(const QString &groupName)
{
    // 预留接口：在此处接入 XAML 聊天气泡绘制逻辑
    // 当前仅记录分组名，后续可根据 groupName 显示对应对话内容
    (void)groupName;
}

} // namespace vpet
