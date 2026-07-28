#include "vpet/speech/voice_input_manager.h"

#include <QAudioDevice>
#include <QAudioInput>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMediaFormat>
#include <QMediaRecorder>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>
#include <QVariant>

namespace vpet
{

namespace
{

const QString GPT_SOVITS_DIRECTORY_NAME = QStringLiteral("GPT-SoVITS");
const QString ASR_SCRIPT_PATH = QStringLiteral("tools/asr/funasr_asr.py");
const QString ASR_LANGUAGE = QStringLiteral("zh");
const QString ASR_MODEL_SIZE = QStringLiteral("large");
const QString ASR_PRECISION = QStringLiteral("float32");
const QString VOICE_INPUT_DIRECTORY_NAME = QStringLiteral("vpet_voice_input");
const QString RECORD_FILE_NAME = QStringLiteral("voice_input.wav");

} // anonymous namespace

VoiceInputManager::VoiceInputManager(QObject *parent)
    : QObject(parent)
    , m_captureSession(new QMediaCaptureSession(this))
    , m_audioInput(nullptr)
    , m_mediaRecorder(new QMediaRecorder(this))
    , m_asrProcess(new QProcess(this))
    , m_recordSessionDirectory()
    , m_recordInputDirectory()
    , m_recordOutputDirectory()
    , m_recordAudioPath()
    , m_asrOutputFilePath()
    , m_isRecording(false)
    , m_awaitingRecorderStop(false)
{
    m_audioInput = new QAudioInput(QMediaDevices::defaultAudioInput(), this);
    m_captureSession->setAudioInput(m_audioInput);
    m_captureSession->setRecorder(m_mediaRecorder);

    QMediaFormat mediaFormat;
    mediaFormat.setFileFormat(QMediaFormat::Wave);
    mediaFormat.setAudioCodec(QMediaFormat::AudioCodec::Wave);
    m_mediaRecorder->setMediaFormat(mediaFormat);
    m_mediaRecorder->setQuality(QMediaRecorder::HighQuality);

    connect(m_mediaRecorder, &QMediaRecorder::errorOccurred,
            this, &VoiceInputManager::OnRecorderError);
    connect(m_mediaRecorder, &QMediaRecorder::recorderStateChanged,
            this, &VoiceInputManager::OnRecorderStateChanged);
    connect(m_asrProcess,
            static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this,
            &VoiceInputManager::OnAsrProcessFinished);
}

VoiceInputManager::~VoiceInputManager()
{
    if (m_mediaRecorder->recorderState() == QMediaRecorder::RecordingState)
    {
        m_mediaRecorder->stop();
    }

    if (m_asrProcess->state() != QProcess::NotRunning)
    {
        m_asrProcess->kill();
        m_asrProcess->waitForFinished(3000);
    }

    CleanupRecordDirectory();
}

bool VoiceInputManager::StartRecording()
{
    if (m_isRecording || m_awaitingRecorderStop)
    {
        emit TranscriptionFailed(QStringLiteral("Voice input is already recording."));
        return false;
    }

    if (m_asrProcess->state() != QProcess::NotRunning)
    {
        emit TranscriptionFailed(QStringLiteral("Voice ASR process is still running."));
        return false;
    }

    QString errorMessage;

    if (!PrepareRecordDirectory(errorMessage))
    {
        emit TranscriptionFailed(errorMessage);
        return false;
    }

    m_mediaRecorder->setOutputLocation(QUrl::fromLocalFile(m_recordAudioPath));
    m_mediaRecorder->record();
    m_isRecording = true;
    m_awaitingRecorderStop = false;
    emit RecordingStarted();

    return true;
}

bool VoiceInputManager::StopRecording()
{
    if (!m_isRecording)
    {
        emit TranscriptionFailed(QStringLiteral("Voice input is not recording."));
        return false;
    }

    if (m_awaitingRecorderStop)
    {
        return true;
    }

    // stop() 是异步的：必须等 recorderStateChanged → StoppedState 后再启动 ASR，
    // 否则 WAV 可能尚未写完，识别会失败或截断。
    m_awaitingRecorderStop = true;
    m_isRecording = false;
    m_mediaRecorder->stop();

    return true;
}

bool VoiceInputManager::IsRecording() const
{
    return m_isRecording;
}

void VoiceInputManager::OnAsrProcessFinished(int exitCode, int exitStatus)
{
    if ((exitStatus != QProcess::NormalExit) || (exitCode != 0))
    {
        const qsizetype standardOutputSize = m_asrProcess->readAllStandardOutput().size();
        const qsizetype standardErrorSize = m_asrProcess->readAllStandardError().size();
        const QString message = QStringLiteral(
                                    "Voice ASR process failed. exit code: %1, stdout bytes: %2, stderr bytes: %3")
                                .arg(exitCode)
                                .arg(standardOutputSize)
                                .arg(standardErrorSize);

        CleanupRecordDirectory();
        emit TranscriptionFailed(message);
        return;
    }

    QString text;
    QString errorMessage;

    if (!ReadTranscriptionText(text, errorMessage))
    {
        CleanupRecordDirectory();
        emit TranscriptionFailed(errorMessage);
        return;
    }

    CleanupRecordDirectory();
    emit TranscriptionCompleted(text);
}

void VoiceInputManager::OnRecorderError()
{
    const QString message = QStringLiteral("Voice recorder error: %1").arg(
                            m_mediaRecorder->errorString());

    m_isRecording = false;
    m_awaitingRecorderStop = false;

    CleanupRecordDirectory();
    emit TranscriptionFailed(message);
}

void VoiceInputManager::OnRecorderStateChanged(QMediaRecorder::RecorderState state)
{
    if (!m_awaitingRecorderStop)
    {
        return;
    }

    if (state != QMediaRecorder::StoppedState)
    {
        return;
    }

    m_awaitingRecorderStop = false;
    emit RecordingStopped(m_recordAudioPath);

    QString errorMessage;

    if (!StartAsrProcess(errorMessage))
    {
        CleanupRecordDirectory();
        emit TranscriptionFailed(errorMessage);
    }
}

bool VoiceInputManager::PrepareRecordDirectory(QString &errorMessage)
{
    if (!CleanupRecordDirectory())
    {
        errorMessage = QStringLiteral("Failed to clean the previous voice input directory.");
        return false;
    }

    const QString baseDirectory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);

    if (baseDirectory.trimmed().isEmpty())
    {
        errorMessage = QStringLiteral("Voice input temp directory is empty.");
        return false;
    }

    const QString timestamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz"));
    m_recordSessionDirectory = QDir(baseDirectory).filePath(
                                   VOICE_INPUT_DIRECTORY_NAME + QStringLiteral("/") + timestamp);
    QDir directory;

    if (!directory.mkpath(m_recordSessionDirectory))
    {
        m_recordSessionDirectory.clear();
        errorMessage = QStringLiteral("Failed to create voice input directory.");
        return false;
    }

    m_recordInputDirectory = QDir(m_recordSessionDirectory).filePath(QStringLiteral("input"));
    m_recordOutputDirectory = QDir(m_recordSessionDirectory).filePath(QStringLiteral("output"));

    if (!directory.mkpath(m_recordInputDirectory) || !directory.mkpath(m_recordOutputDirectory))
    {
        CleanupRecordDirectory();
        errorMessage = QStringLiteral("Failed to create voice ASR working directories.");
        return false;
    }

    m_recordAudioPath = QDir(m_recordInputDirectory).filePath(RECORD_FILE_NAME);
    m_asrOutputFilePath = QDir(m_recordOutputDirectory).filePath(
                          QFileInfo(m_recordInputDirectory).fileName() + QStringLiteral(".list"));

    return true;
}

bool VoiceInputManager::StartAsrProcess(QString &errorMessage)
{
    if (m_recordAudioPath.trimmed().isEmpty() || !QFileInfo::exists(m_recordAudioPath))
    {
        errorMessage = QStringLiteral("Voice input audio file does not exist.");
        return false;
    }

    const QString gptSoVitsRootPath = FindGptSoVitsRootPath();

    if (gptSoVitsRootPath.isEmpty())
    {
        errorMessage = QStringLiteral("GPT-SoVITS directory is not found.");
        return false;
    }

    const QString scriptPath = QDir(gptSoVitsRootPath).filePath(ASR_SCRIPT_PATH);

    if (!QFileInfo::exists(scriptPath))
    {
        errorMessage = QStringLiteral("GPT-SoVITS ASR script is not found.");
        return false;
    }

    const QString pythonExecutable = FindPythonExecutable();

    if (pythonExecutable.trimmed().isEmpty())
    {
        errorMessage = QStringLiteral("Python executable is not found for voice ASR.");
        return false;
    }

    QStringList arguments;
    arguments.append(QStringLiteral("-s"));
    arguments.append(scriptPath);
    arguments.append(QStringLiteral("-i"));
    arguments.append(m_recordInputDirectory);
    arguments.append(QStringLiteral("-o"));
    arguments.append(m_recordOutputDirectory);
    arguments.append(QStringLiteral("-s"));
    arguments.append(ASR_MODEL_SIZE);
    arguments.append(QStringLiteral("-l"));
    arguments.append(ASR_LANGUAGE);
    arguments.append(QStringLiteral("-p"));
    arguments.append(ASR_PRECISION);

    m_asrProcess->setWorkingDirectory(gptSoVitsRootPath);
    m_asrProcess->start(pythonExecutable, arguments);

    if (!m_asrProcess->waitForStarted(3000))
    {
        errorMessage = QStringLiteral("Failed to start GPT-SoVITS ASR process.");
        return false;
    }

    return true;
}

QString VoiceInputManager::FindGptSoVitsRootPath() const
{
    const QString applicationDirectory = QCoreApplication::applicationDirPath();
    const QStringList candidatePaths =
    {
        QDir(applicationDirectory).filePath(GPT_SOVITS_DIRECTORY_NAME),
        QDir::current().filePath(GPT_SOVITS_DIRECTORY_NAME),
        QDir(applicationDirectory).filePath(QStringLiteral("../") + GPT_SOVITS_DIRECTORY_NAME),
        QDir(applicationDirectory).filePath(QStringLiteral("../../") + GPT_SOVITS_DIRECTORY_NAME)
    };

    for (const QString &candidatePath : candidatePaths)
    {
        const QFileInfo fileInfo(candidatePath);

        if (fileInfo.exists() && fileInfo.isDir())
        {
            return fileInfo.absoluteFilePath();
        }
    }

    return QString();
}

QString VoiceInputManager::FindPythonExecutable() const
{
    const QString gptSoVitsRootPath = FindGptSoVitsRootPath();
    const QStringList candidatePaths =
    {
        QDir(gptSoVitsRootPath).filePath(QStringLiteral("runtime/python.exe")),
        QStringLiteral("python")
    };

    for (const QString &candidatePath : candidatePaths)
    {
        if (candidatePath == QStringLiteral("python"))
        {
            return candidatePath;
        }

        if (QFileInfo::exists(candidatePath))
        {
            return QFileInfo(candidatePath).absoluteFilePath();
        }
    }

    return QString();
}

bool VoiceInputManager::ReadTranscriptionText(QString &text, QString &errorMessage) const
{
    text.clear();

    if (m_asrOutputFilePath.trimmed().isEmpty() || !QFileInfo::exists(m_asrOutputFilePath))
    {
        errorMessage = QStringLiteral("Voice ASR output file is not found.");
        return false;
    }

    QFile file(m_asrOutputFilePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        errorMessage = QStringLiteral("Failed to open voice ASR output file.");
        return false;
    }

    const QString outputText = QString::fromUtf8(file.readAll()).trimmed();
    file.close();

    if (outputText.isEmpty())
    {
        errorMessage = QStringLiteral("Voice ASR output is empty.");
        return false;
    }

    const QStringList outputLines = outputText.split(QStringLiteral("\n"), Qt::SkipEmptyParts);

    if (outputLines.isEmpty())
    {
        errorMessage = QStringLiteral("Voice ASR output line is empty.");
        return false;
    }

    const QStringList fields = outputLines.first().split(QStringLiteral("|"));

    if (fields.size() < 4)
    {
        errorMessage = QStringLiteral("Voice ASR output format is invalid.");
        return false;
    }

    text = fields.mid(3).join(QStringLiteral("|")).trimmed();

    if (text.isEmpty())
    {
        errorMessage = QStringLiteral("Voice ASR transcription text is empty.");
        return false;
    }

    return true;
}

bool VoiceInputManager::CleanupRecordDirectory()
{
    if (!m_recordSessionDirectory.isEmpty())
    {
        QDir sessionDirectory(m_recordSessionDirectory);

        if (sessionDirectory.exists() && !sessionDirectory.removeRecursively())
        {
            qWarning() << "[VoiceInput] Failed to remove temporary session directory.";
            return false;
        }
    }

    m_recordSessionDirectory.clear();
    m_recordInputDirectory.clear();
    m_recordOutputDirectory.clear();
    m_recordAudioPath.clear();
    m_asrOutputFilePath.clear();

    return true;
}

} // namespace vpet
