#pragma once

#include <QAudioInput>
#include <QMediaCaptureSession>
#include <QMediaRecorder>
#include <QObject>
#include <QQmlEngine>
#include <QTemporaryDir>

#include <memory>

namespace jot {

/// Records from the desktop's microphone into a file of its own, for a note to attach: Ogg
/// Opus where the system can write it, else WAV.
class AudioRecorder : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    /// Whether there is a microphone to record from.
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    /// How long the recording is so far, in milliseconds.
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)

public:
    explicit AudioRecorder(QObject *parent = nullptr);

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool recording() const;
    [[nodiscard]] qint64 duration() const { return m_recorder.duration(); }

    /// Starts recording; `name` is what the file is called, without its extension.
    Q_INVOKABLE void start(const QString &name);
    /// Stops; finished() gives the file once it is written.
    Q_INVOKABLE void stop();

Q_SIGNALS:
    void availableChanged();
    void recordingChanged();
    void durationChanged();
    /// The recording is in the file at `url`, for the note to take.
    void finished(const QUrl &url);
    void failed(const QString &why);

private:
    QMediaCaptureSession m_session;
    QAudioInput m_input;
    QMediaRecorder m_recorder;
    std::unique_ptr<QTemporaryDir> m_folder;
};

} // namespace jot
