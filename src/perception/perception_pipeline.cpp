#include "vpet/perception/perception_pipeline.h"

#include <QDateTime>
#include <QSize>
#include <QtGlobal>

namespace vpet
{

namespace
{

constexpr std::size_t DEFAULT_BUFFER_CAPACITY = 5;

} // anonymous namespace

PerceptionPipeline::PerceptionPipeline(const _tagConfig &config, QObject *parent)
    : QObject(parent)
    , m_config(NormalizeConfig(config))
    , m_sensor(new ScreenshotSensor(m_config.sensorConfig, this))
    , m_frameBuffer(m_config.bufferCapacity)
    , m_processors()
    , m_latestEncodedData()
    , m_latestFrameSize()
    , m_latestFrameId(-1)
    , m_isRunning(false)
{
    connect(m_sensor,
            &ScreenshotSensor::FrameCaptured,
            this,
            &PerceptionPipeline::OnFrameCaptured);
    connect(m_sensor,
            &ScreenshotSensor::ErrorOccurred,
            this,
            &PerceptionPipeline::OnSensorErrorOccurred);
}

PerceptionPipeline::~PerceptionPipeline()
{
    Stop();
}

bool PerceptionPipeline::Start()
{
    if (m_isRunning)
    {
        return true;
    }

    if (m_sensor == nullptr)
    {
        emit ErrorOccurred(QStringLiteral("Perception sensor is not available."));
        return false;
    }

    const bool started = m_sensor->Start();

    if (!started)
    {
        emit ErrorOccurred(QStringLiteral("Perception sensor failed to start."));
        return false;
    }

    m_isRunning = true;

    return true;
}

void PerceptionPipeline::Stop()
{
    if (m_sensor != nullptr)
    {
        m_sensor->Stop();
    }

    m_isRunning = false;
}

bool PerceptionPipeline::IsRunning() const
{
    return m_isRunning;
}

bool PerceptionPipeline::CaptureOnce()
{
    if (m_sensor == nullptr)
    {
        emit ErrorOccurred(QStringLiteral("Perception sensor is not available."));
        return false;
    }

    return m_sensor->CaptureOnce();
}

QByteArray PerceptionPipeline::GetLatestEncodedData() const
{
    return m_latestEncodedData;
}

QSize PerceptionPipeline::GetLatestFrameSize() const
{
    return m_latestFrameSize;
}

QVector<QByteArray> PerceptionPipeline::GetRecentEncodedData(std::size_t count) const
{
    QVector<QByteArray> encodedBatch;

    if ((count == 0) || !m_config.enableBuffer)
    {
        return encodedBatch;
    }

    const QVector<_tagFrame> recentFrames = m_frameBuffer.GetRecent(count);
    encodedBatch.reserve(recentFrames.size());

    for (const _tagFrame &frame : recentFrames)
    {
        if (frame.pixmap.isNull())
        {
            continue;
        }

        const QByteArray encodedData = EncodePixmap(frame.pixmap);

        if (!encodedData.isEmpty())
        {
            encodedBatch.append(encodedData);
        }
    }

    return encodedBatch;
}

std::size_t PerceptionPipeline::GetBufferedFrameCount() const
{
    if (!m_config.enableBuffer)
    {
        return 0;
    }

    return m_frameBuffer.GetSize();
}

bool PerceptionPipeline::AddProcessor(const Processor &processor)
{
    if (!processor)
    {
        return false;
    }

    m_processors.append(processor);

    return true;
}

void PerceptionPipeline::ClearProcessors()
{
    m_processors.clear();
}

void PerceptionPipeline::OnFrameCaptured(const QByteArray &base64Data,
                                         int frameCount,
                                         const QSize &frameSize)
{
    if (base64Data.isEmpty())
    {
        emit ErrorOccurred(QStringLiteral("Perception sensor returned empty Base64 data."));
        return;
    }

    if ((frameCount <= 0) || !frameSize.isValid())
    {
        emit ErrorOccurred(QStringLiteral("Perception sensor returned invalid frame metadata."));
        return;
    }

    QString errorMessage;

    if (!ProcessLatestSensorFrame(frameCount, errorMessage))
    {
        emit ErrorOccurred(errorMessage);
    }
}

void PerceptionPipeline::OnSensorErrorOccurred(const QString &message)
{
    const QString normalizedMessage = message.trimmed();

    if (normalizedMessage.isEmpty())
    {
        emit ErrorOccurred(QStringLiteral("Perception sensor reported an empty error."));
        return;
    }

    emit ErrorOccurred(normalizedMessage);
}

PerceptionPipeline::_tagConfig PerceptionPipeline::NormalizeConfig(const _tagConfig &config)
{
    _tagConfig normalizedConfig = config;

    if (normalizedConfig.bufferCapacity == 0)
    {
        normalizedConfig.bufferCapacity = DEFAULT_BUFFER_CAPACITY;
    }

    normalizedConfig.sensorConfig.autoStart = false;

    return normalizedConfig;
}

QPixmap PerceptionPipeline::ApplyProcessors(const QPixmap &pixmap) const
{
    if (pixmap.isNull())
    {
        return QPixmap();
    }

    QPixmap processedPixmap = pixmap;

    for (const Processor &processor : m_processors)
    {
        if (!processor)
        {
            return QPixmap();
        }

        processedPixmap = processor(processedPixmap);

        if (processedPixmap.isNull())
        {
            return QPixmap();
        }
    }

    return processedPixmap;
}

bool PerceptionPipeline::ProcessLatestSensorFrame(int frameCount, QString &errorMessage)
{
    if (frameCount <= 0)
    {
        errorMessage = QStringLiteral("Perception frame count is invalid.");
        return false;
    }

    if (m_sensor == nullptr)
    {
        errorMessage = QStringLiteral("Perception sensor is not available.");
        return false;
    }

    const QPixmap sensorPixmap = m_sensor->GetLatestFrame();

    if (sensorPixmap.isNull())
    {
        errorMessage = QStringLiteral("Perception sensor latest frame is empty.");
        return false;
    }

    const QPixmap processedPixmap = ApplyProcessors(sensorPixmap);

    if (processedPixmap.isNull())
    {
        errorMessage = QStringLiteral("Perception processor returned an empty frame.");
        return false;
    }

    const QByteArray encodedData = EncodePixmap(processedPixmap);

    if (encodedData.isEmpty())
    {
        errorMessage = QStringLiteral("Perception failed to encode processed frame.");
        return false;
    }

    if (m_config.enableBuffer)
    {
        _tagFrame frame;
        frame.pixmap = processedPixmap;
        frame.timestamp = QDateTime::currentDateTimeUtc();
        frame.sequenceId = frameCount;
        frame.filePath = m_sensor->GetLatestFilePath();

        if (!m_frameBuffer.Push(frame))
        {
            errorMessage = QStringLiteral("Perception failed to buffer processed frame.");
            return false;
        }
    }

    m_latestEncodedData = encodedData;
    m_latestFrameSize = processedPixmap.size();
    m_latestFrameId = frameCount;

    emit DataReady(m_latestEncodedData, m_latestFrameId);

    if (m_config.enableBuffer)
    {
        const QVector<QByteArray> batch = GetRecentEncodedData(m_frameBuffer.GetSize());

        if (!batch.isEmpty())
        {
            emit BatchReady(batch, m_latestFrameId);
        }
    }

    return true;
}

QByteArray PerceptionPipeline::EncodePixmap(const QPixmap &pixmap) const
{
    if (pixmap.isNull())
    {
        return QByteArray();
    }

    return VisionEncoder::Encode(pixmap, m_config.encodeFormat, m_config.encodeOptions);
}

} // namespace vpet
