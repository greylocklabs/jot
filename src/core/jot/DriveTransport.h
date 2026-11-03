#pragma once

#include "jot/Sync.h"

#include <QHash>
#include <QJsonObject>
#include <QMap>
#include <QUrl>

#include <atomic>
#include <memory>

class QNetworkAccessManager;
class QTcpServer;

namespace jot {

/// Google's addresses, which tests point elsewhere.
struct DriveApi
{
    QUrl api{QStringLiteral("https://www.googleapis.com")};
    QUrl token{QStringLiteral("https://oauth2.googleapis.com/token")};
    QUrl authorize{QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth")};
};

/// The OAuth client Google knows this build of Jot by, from the build; empty when it was built
/// without one, and so cannot sync with Google Drive.
[[nodiscard]] QString googleClientId();
[[nodiscard]] QString googleClientSecret();

/// Signing in to Google as an app on a computer: PKCE, with Google sending the browser back to
/// a listener on this computer's loopback address, which takes the code from it.
class DriveSignIn
{
public:
    explicit DriveSignIn(DriveApi api = DriveApi());
    ~DriveSignIn();

    /// Starts listening, and gives where to send the user; empty when nothing can listen.
    QUrl start(const QString &clientId);
    /// Waits for the browser to come back, up to `timeoutMs` or until `cancel` is set, and
    /// trades its code for a refresh token, which lasts until revoked; or nothing, with `error`
    /// saying why.
    std::optional<QString> finish(const QString &clientId, const QString &clientSecret,
                                  int timeoutMs, QString *error,
                                  const std::atomic<bool> *cancel = nullptr);

private:
    DriveApi m_api;
    std::unique_ptr<QTcpServer> m_server;
    QString m_verifier;
    QString m_state;
    QString m_redirect;
};

/// A vault's files in a folder of Jot's own in the user's Google Drive, as files in folders by
/// their paths, readable there without Jot. Jot sees only what it made there, so it asks for
/// nothing more. Drive's own trash keeps what a sync deletes.
///
/// Drive cannot refuse a write over a version other than the one expected, so Jot checks the
/// version just before writing: two computers writing the same file within that moment could
/// still overwrite each other.
class DriveTransport : public Transport
{
public:
    /// `folder` is the id of the Drive folder the vault is kept in.
    DriveTransport(QString clientId, QString clientSecret, QString refreshToken, QString folder,
                   DriveApi api = DriveApi());
    ~DriveTransport() override;

    std::optional<QList<RemoteFile>> list(QString *error) override;
    std::optional<QByteArray> get(const QString &path, QString *rev, QString *error) override;
    std::optional<QString> put(const QString &path, const QByteArray &data, const QString &expected,
                               bool *stale, QString *error) override;
    bool remove(const QString &path, const QString &expected, bool *stale, QString *error) override;

    /// The id of the folder called `name` at the top of the user's Drive that Jot made, making
    /// it when there is none.
    std::optional<QString> folderNamed(const QString &name, QString *error);

    static constexpr auto kFolderType = "application/vnd.google-apps.folder";

private:
    struct Reply
    {
        int status = 0;
        QByteArray body;
        QString error;
    };
    struct Entry
    {
        QString id;
        QString version;
    };
    Reply call(const QByteArray &method, const QUrl &url, const QByteArray &body = {},
               const QByteArray &contentType = {});
    [[nodiscard]] QUrl apiUrl(const QString &path,
                              const QList<std::pair<QString, QString>> &query = {}) const;
    bool authorize(QString *error);
    /// Lists once, so paths can be found by id.
    bool known(QString *error);
    /// The version Drive holds of the file with `id` now.
    std::optional<QString> versionOf(const QString &id, QString *error);
    /// The id of what Drive holds called `name` in the folder `parent`, a folder or a file as
    /// `folder` says; empty when there is none.
    std::optional<QString> childNamed(const QString &parent, const QString &name, bool folder,
                                      QString *error);
    /// The id of the folder at `path` from the vault's root, making the folders on the way.
    std::optional<QString> folderFor(const QString &path, QString *error);
    [[nodiscard]] static QString problem(const Reply &reply);

    QString m_clientId;
    QString m_clientSecret;
    QString m_refreshToken;
    QString m_folder;
    DriveApi m_api;
    QString m_accessToken;
    bool m_listed = false;
    QHash<QString, Entry> m_files;
    QHash<QString, QString> m_folders;
    std::unique_ptr<QNetworkAccessManager> m_network;
};

} // namespace jot
