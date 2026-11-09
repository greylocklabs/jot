#include "AudioRecorder.h"

#include <QAudioDevice>
#include <QMediaDevices>
#include <QMediaFormat>

using namespace Qt::StringLiterals;

namespace jot {

AudioRecorder::AudioRecorder(QObject *parent) : QObject(parent)
{
    m_session.setAudioInput(&m_input);
    m_session.setRecorder(&m_recorder);
    connect(&m_recorder, &QMediaRecorder::recorderStateChanged, this, [this](auto state) {
        Q_EMIT recordingChanged();
        if (state == QMediaRecorder::StoppedState &&
            m_recorder.error() == QMediaRecorder::NoError && !m_recorder.actualLocation().isEmpty())
            Q_EMIT finished(m_recorder.actualLocation());
    });
    connect(&m_recorder, &QMediaRecorder::durationChanged, this, &AudioRecorder::durationChanged);
    connect(&m_recorder, &QMediaRecorder::errorOccurred, this,
            [this](QMediaRecorder::Error, const QString &why) { Q_EMIT failed(why); });
    auto *devices = new QMediaDevices(this);
    connect(devices, &QMediaDevices::audioInputsChanged, this, [this] {
        m_input.setDevice(QMediaDevices::defaultAudioInput());
        Q_EMIT availableChanged();
    });
}

bool AudioRecorder::available() const
{
    return !QMediaDevices::defaultAudioInput().isNull();
}

bool AudioRecorder::recording() const
{
    return m_recorder.recorderState() == QMediaRecorder::RecordingState;
}

void AudioRecorder::start(const QString &name)
{
    if (recording())
        return;
    m_folder = std::make_unique<QTemporaryDir>();
    QMediaFormat format(QMediaFormat::Ogg);
    format.setAudioCodec(QMediaFormat::AudioCodec::Opus);
    QString extension = u"ogg"_s;
    if (!format.isSupported(QMediaFormat::Encode)) {
        format = QMediaFormat(QMediaFormat::Wave);
        format.setAudioCodec(QMediaFormat::AudioCodec::Wave);
        extension = u"wav"_s;
    }
    m_recorder.setMediaFormat(format);
    m_recorder.setQuality(QMediaRecorder::HighQuality);
    m_recorder.setOutputLocation(QUrl::fromLocalFile(m_folder->filePath(name + u'.' + extension)));
    m_input.setDevice(QMediaDevices::defaultAudioInput());
    m_recorder.record();
}

void AudioRecorder::stop()
{
    m_recorder.stop();
}

} // namespace jot
