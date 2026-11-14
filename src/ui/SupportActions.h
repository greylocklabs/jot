#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QQmlEngine>

namespace jot {

/// The title bar's help menu: debug info, the logs folder and problem reports.
class SupportActions : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Support)
    QML_SINGLETON
    /// Whether web videos play in the note, as built with Qt WebEngine; else in the browser.
    Q_PROPERTY(bool webVideo READ webVideo CONSTANT)

public:
    using QObject::QObject;

    [[nodiscard]] static bool webVideo()
    {
#if JOT_WEB_VIDEO
        return true;
#else
        return false;
#endif
    }

    /// Puts the `jot doctor` report on the clipboard, then emits copied().
    Q_INVOKABLE void copyDebugInfo();
    Q_INVOKABLE void openLogs();
    /// Opens the vault's folder ("vault"), or the folder holding the settings ("config")
    /// or the search index ("cache").
    Q_INVOKABLE void openFolder(const QString &which);
    /// Puts `text` on the clipboard, then emits copied().
    Q_INVOKABLE void copyText(const QString &text);
    /// The syntax guide, data/syntax.md, as Markdown.
    Q_INVOKABLE static QString syntaxGuide();
    /// Opens the report as a new GitHub issue, for the person to submit there.
    Q_INVOKABLE void reportOnGitHub(bool idea, const QString &summary, const QString &note,
                                    const QString &diagnostics);
    /// Gathers the `jot doctor` report for "Report a problem" to show, then emits
    /// diagnosticsReady().
    Q_INVOKABLE void gatherDiagnostics();
    /// Sends a problem report to the report service, then emits reportSent() with the new
    /// issue, or reportFailed() with why it was not filed.
    Q_INVOKABLE void sendReport(bool idea, const QString &summary, const QString &note,
                                const QString &diagnostics);

Q_SIGNALS:
    void copied();
    void diagnosticsReady(const QString &text);
    void reportSent(const QUrl &issue);
    void reportFailed(const QString &error);

private:
    [[nodiscard]] static QString collect();

    QNetworkAccessManager m_network;
};

} // namespace jot
