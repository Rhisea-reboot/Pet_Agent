# Pet Agent 视觉感知框架

## 设计原则

- **无 UI 依赖**：仅使用 QtCore + QtGui
- **纯后台运行**：QTimer 驱动，无窗口
- **模块化**：可插拔感知器，统一接口
- **数据流**：屏幕 → 帧缓冲 → 编码器 → Agent

---

## 模块划分

```
┌─────────────────────────────────────────────┐
│  Agent（核心）                                │
│  - 模块注册中心                                │
│  - 生命周期管理                                │
│  - 配置中心                                   │
└──────────────────┬──────────────────────────┘
                   │ owns
                   ▼
┌─────────────────────────────────────────────┐
│  PerceptionPipeline（感知管道）               │
│  - 协调传感器、缓冲、编码、输出                 │
│  - 支持中间件处理链                            │
└──────┬──────────────────┬───────────────────┘
       │ owns             │ owns
       ▼                  ▼
┌──────────────┐  ┌──────────────┐
│  Sensor      │  │  FrameBuffer │
│  (ISensor)   │  │  (环形缓冲)   │
└──────┬───────┘  └──────────────┘
       │
       ▼
┌──────────────┐
│ VisionEncoder│
│ (图像编码)    │
└──────────────┘
```

---

## 接口定义

### 1. IModule — 模块基类

所有 Agent 模块继承此接口。

```cpp
class IModule : public QObject {
    virtual QString name() const = 0;      // 模块标识名
    virtual QString version() const = 0;  // 版本号
    virtual void initialize();              // 初始化
    virtual void shutdown();                // 清理
};
```

### 2. ISensor — 感知器接口

所有感官输入（视觉、音频、网络等）继承此接口。

```cpp
class ISensor : public IModule {
    virtual void start() = 0;              // 开始感知
    virtual void stop() = 0;               // 停止感知
    virtual bool isRunning() const = 0;    // 是否运行中

signals:
    void perceptionReady(const QByteArray& data, const QString& modality);
    void error(const QString& msg);
};
```

### 3. ScreenshotSensor — 视觉感知器

**职责**：定时全屏截图，输出 QPixmap / 字节流 / Base64。

```cpp
class ScreenshotSensor : public ISensor {

    // ─── 配置 ───
    struct Config {
        int intervalMs;              // 截图间隔（毫秒）
        int maxFrames;               // 最大帧数，-1 无限
        bool captureAllScreens;      // 是否拼接多屏
        bool saveToDisk;             // 是否保存到文件
        QString saveDir;             // 保存目录
        QString format;              // 图像格式：PNG / JPG / BMP
        int quality;                 // 压缩质量
        QString prefix;              // 文件名前缀
        bool autoStart;              // 构造后自动启动
    };

    // ─── 构造 ───
    ScreenshotSensor(const Config& cfg, QObject* parent);

    // ─── ISensor 实现 ───
    void start();                    // 启动定时器，立即先截一张
    void stop();                     // 停止定时器
    bool isRunning() const;

    // ─── 截图 ───
    QString captureOnce();           // 单次截图，返回文件路径

    // ─── 数据访问 ───
    QPixmap latestFrame() const;                    // 最新帧图像
    QByteArray latestFrameBytes(const QString& fmt) const;   // 字节流
    QByteArray latestFrameBase64(const QString& fmt) const;  // Base64
    QSize frameSize() const;                        // 帧尺寸
    int frameCount() const;                           // 已截图数量
    QString latestFilePath() const;                   // 最新文件路径

    // ─── 热更新 ───
    void setInterval(int ms);        // 修改间隔（运行时生效）
    void setMaxFrames(int max);      // 修改最大帧数
    void setSaveToDisk(bool save);   // 修改保存策略

signals:
    void frameCaptured(const QPixmap& frame, int count, const QString& path);
};
```

### 4. FrameBuffer — 帧缓冲

**职责**：环形存储最近 N 帧，支持时序回溯。

```cpp
struct Frame {
    QPixmap pixmap;          // 图像数据
    QDateTime timestamp;    // 时间戳
    int sequenceId;         // 序列号
    QString filePath;       // 文件路径（如有）
};

class FrameBuffer {
    FrameBuffer(size_t capacity);     // 容量

    void push(const Frame& frame);    // 写入一帧
    Frame latest() const;             // 最新帧
    Frame at(size_t index) const;     // 索引：0=最新，1=次新...
    QVector<Frame> recent(size_t n) const;   // 最近 N 帧
    size_t size() const;              // 当前数量
    void clear();                     // 清空

    QPixmap stitchRecent(size_t n, Qt::Orientation orient) const;  // 拼接长图
};
```

### 5. VisionEncoder — 视觉编码器

**职责**：将 QPixmap 转换为模型输入格式。

```cpp
class VisionEncoder {

    enum class Format {
        RawPNG,         // PNG 原始字节
        RawJPEG,        // JPEG 原始字节
        Base64PNG,      // Base64 PNG（LLM API 常用）
        Base64JPEG,     // Base64 JPEG
        Grayscale,      // 单通道灰度
        RGB888,         // RGB 原始字节
    };

    struct EncodeOptions {
        int maxWidth;           // 最大宽度，0=不缩放
        int maxHeight;          // 最大高度，0=不缩放
        int quality;            // JPEG 质量 0-100
        bool keepAspectRatio;   // 保持宽高比
    };

    // 编码为指定格式
    static QByteArray encode(const QPixmap& pixmap, Format fmt,
                             const EncodeOptions& opts = {});

    // 快捷方法
    static QByteArray toBase64(const QPixmap& pixmap,
                                const QString& format = "PNG", int quality = -1);
    static QByteArray toGrayscale(const QPixmap& pixmap,
                                   int w = 0, int h = 0);

    // LLM API 辅助
    static QString makeOpenAIVisionUrl(const QByteArray& base64Png);
    static QByteArray makeClaudeMediaPayload(const QPixmap& pixmap,
                                                const QString& mediaType = "image/png");
};
```

### 6. PerceptionPipeline — 感知管道

**职责**：整合传感器 → 缓冲 → 编码 → 输出，支持处理链。

```cpp
class PerceptionPipeline : public QObject {

    // ─── 配置 ───
    struct Config {
        ScreenshotSensor::Config sensorConfig;     // 传感器配置
        size_t bufferCapacity;                      // 缓冲容量
        VisionEncoder::Format encodeFormat;         // 编码格式
        VisionEncoder::EncodeOptions encodeOptions;   // 编码选项
        bool enableBuffer;                          // 是否启用缓冲
    };

    // ─── 构造 ───
    PerceptionPipeline(const Config& cfg, QObject* parent);

    // ─── 控制 ───
    void start();                    // 启动管道
    void stop();                     // 停止管道
    bool isRunning() const;

    // ─── 数据获取 ───
    QByteArray latestEncodedData() const;                    // 最新编码数据
    QVector<QByteArray> recentEncodedData(size_t n) const;   // 最近 N 帧编码数据

    // ─── 处理链 ───
    using Processor = std::function<QPixmap(const QPixmap&)>;
    void addProcessor(Processor proc);    // 添加图像处理中间件
    void clearProcessors();                // 清空处理链

signals:
    void dataReady(const QByteArray& encodedData, int frameId);
    void batchReady(const QVector<QByteArray>& batch, int latestFrameId);
    void error(const QString& msg);
};
```

### 7. Agent — 核心

**职责**：模块注册、生命周期管理、配置中心。

```cpp
class Agent : public QObject {

    Agent(QObject* parent);

    // ─── 模块管理 ───
    void registerModule(const QString& name, IModule* module);
    IModule* module(const QString& name) const;
    template<typename T> T* moduleAs(const QString& name) const;

    // ─── 快捷访问 ───
    PerceptionPipeline* vision() const;    // 视觉感知管道

    // ─── 生命周期 ───
    void initialize();
    void start();
    void stop();
    bool isRunning() const;

    // ─── 配置 ───
    void setConfig(const QString& key, const QVariant& value);
    QVariant config(const QString& key) const;

signals:
    void initialized();
    void started();
    void stopped();
    void perceptionReceived(const QByteArray& data, const QString& modality);
    void error(const QString& module, const QString& msg);
};
```

---

## 数据流

```
Screen ──[grabWindow]──▶ QPixmap ──[Processor链]──▶ QPixmap
                                                       │
                                                       ▼
                                              ┌────────────────┐
                                              │  FrameBuffer   │
                                              │  (环形缓冲)     │
                                              └───────┬────────┘
                                                      │
                                                      ▼
                                              ┌────────────────┐
                                              │ VisionEncoder  │
                                              │ encode()       │
                                              └───────┬────────┘
                                                      │
                                                      ▼
                                              ┌────────────────┐
                                              │ QByteArray     │
                                              │ (Base64/Raw)   │
                                              └───────┬────────┘
                                                      │
                                                      ▼
                                              ┌────────────────┐
                                              │ Agent          │
                                              │ perceptionReceived()
                                              └────────────────┘
                                                      │
                                                      ▼
                                              ┌────────────────┐
                                              │ LLM Vision API │
                                              │ (GPT-4o/Claude)│
                                              └────────────────┘
```

---

## 目录结构

```
pet_agent/
├── CMakeLists.txt
├── include/
│   └── pet_agent/
│       ├── core/
│       │   ├── module.h
│       │   └── agent.h
│       ├── sensor/
│       │   ├── sensor.h
│       │   └── screenshot_sensor.h
│       └── perception/
│           ├── frame_buffer.h
│           ├── vision_encoder.h
│           └── perception_pipeline.h
├── src/
│   ├── core/
│   │   └── agent.cpp
│   ├── sensor/
│   │   └── screenshot_sensor.cpp
│   └── perception/
│       ├── frame_buffer.cpp
│       ├── vision_encoder.cpp
│       └── perception_pipeline.cpp
└── examples/
    └── cli/
        └── main.cpp
```

---

## 使用范式

```cpp
// 1. 创建 Agent
Agent agent;

// 2. 配置视觉管道
PerceptionPipeline::Config cfg;
cfg.sensorConfig.intervalMs = 3000;       // 3 秒一帧
cfg.sensorConfig.saveToDisk = false;    // 纯内存
cfg.encodeFormat = VisionEncoder::Format::Base64PNG;

auto* vision = new PerceptionPipeline(cfg, &agent);
agent.registerModule("vision", vision);

// 3. 可选：添加图像处理中间件
vision->addProcessor([](const QPixmap& frame) {
    return frame.scaled(1920, 1080, Qt::KeepAspectRatio);
});

// 4. 接收感知数据
connect(&agent, &Agent::perceptionReceived,
        [](const QByteArray& data, const QString& modality) {
    // data 即 Base64，直接构造 LLM payload
});

// 5. 启动
agent.initialize();
agent.start();
```
