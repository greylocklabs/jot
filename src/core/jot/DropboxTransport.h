#pragma once

#include "jot/Sync.h"

#include <QJsonObject>
#include <QMap>
#include <QUrl>

#include <memory>

class QNetworkAccessManager;

namespace jot {

/// Dropbox's addresses, which tests point elsewhere.
struct DropboxApi
{
    QUrl api{QStringLiteral("https://api.dropboxapi.com")};
    QUrl content{QStringLiteral("https://content.dropboxapi.com")};
    QUrl authorize{QStringLiteral("https://www.dropbox.com/oauth2/authorize")};
};

/// The app key Dropbox knows this build of Jot by, from the build; empty when it was built
/// without one, and so cannot sync with Dropbox.
[[nodiscard]] QString dropboxAppKey();

/// Signing in to Dropbox as an app on a computer: PKCE with no redirect, so Dropbox shows a code
/// to paste back rather than calling a server Jot would have to run.
struct DropboxSignIn
{
    /// Where to send the user to allow Jot into its app folder.
    QUrl url;
    /// The PKCE verifier, kept until the code comes back.
    QString verifier;

    static DropboxSignIn start(const QString &appKey, const DropboxApi &api = {});
    /// Trades the code Dropbox showed for a refresh token, which lasts until revoked; or
    /// nothing, with `error` saying why.
    [[nodiscard]] std::optional<QString> finish(const QString &appKey, const QString &code,
                                                QString *error, const DropboxApi &api = {}) const;
};

/// A vault's files in Jot's own folder in the user's Dropbox, by their paths, readable there
/// without Jot. Writes name the revision they replace, and Dropbox keeps what is deleted for a
/// while, as its own trash.
class DropboxTransport : public Transport
{
public:
    /// `folder` is where in the app folder the vault goes, empty for its top.
    DropboxTransport(QString appKey, QString refreshToken, QString folder,
                     DropboxApi api = DropboxApi());
    ~DropboxTransport() override;

    std::optional<QList<RemoteFile>> list(QString *error) override;
    std::optional<QByteArray> get(const QString &path, QString *rev, QString *error) override;
    std::optional<QString> put(const QString &path, const QByteArray &data, const QString &expected,
                               bool *stale, QString *error) override;
    bool remove(const QString &path, const QString &expected, bool *stale, QString *error) override;

    /// Above this, Dropbox wants an upload in parts, which Jot does not do yet.
    static constexpr qint64 kMaxUpload = 150 * 1024 * 1024;

private:
    struct Reply
    {
        int status = 0;
        QByteArray body;
        QMap<QByteArray, QByteArray> headers;
        QString error;
    };
    Reply post(const QUrl &url, const QByteArray &body,
               const QMap<QByteArray, QByteArray> &headers);
    /// Calls an RPC endpoint with `arguments`, getting an access token first if need be.
    Reply rpc(const QString &endpoint, const QJsonObject &arguments, QString *error);
    /// Calls a content endpoint, the arguments in the Dropbox-API-Arg header.
    Reply content(const QString &endpoint, const QJsonObject &arguments, const QByteArray &body,
                  QString *error);
    bool authorize(QString *error);
    [[nodiscard]] QString dropboxPath(const QString &path) const;
    [[nodiscard]] static QString problem(const Reply &reply);

    QString m_appKey;
    QString m_refreshToken;
    QString m_folder;
    DropboxApi m_api;
    QString m_accessToken;
    std::unique_ptr<QNetworkAccessManager> m_network;
};

} // namespace jot
