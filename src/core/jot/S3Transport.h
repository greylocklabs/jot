#pragma once

#include "jot/Sync.h"

#include <QMap>
#include <QUrl>

#include <memory>

class QNetworkAccessManager;

namespace jot {

/// Where in an S3-compatible store a vault syncs: AWS, Cloudflare R2, Backblaze B2, MinIO,
/// Wasabi and the like.
struct S3Location
{
    /// The service's address, such as `https://s3.us-east-1.amazonaws.com`.
    QUrl endpoint;
    QString region;
    QString bucket;
    /// The folder in the bucket the vault's files go under, without slashes at either end;
    /// empty for the bucket's top.
    QString prefix;
    /// The bucket in the path, as MinIO and most others take it, rather than in the host name.
    bool pathStyle = true;

    /// Read from a sync setting, `s3:<endpoint>/<bucket>[/<prefix>]?region=<region>[&style=
    /// virtual]`; nothing when it is not one.
    static std::optional<S3Location> parse(const QString &remote);
    /// As a sync setting writes it.
    [[nodiscard]] QString remote() const;
};

/// A vault's files as objects in an S3-compatible bucket, each under the prefix by its path, so
/// they can be read without Jot. Writes are conditional on the object's ETag, as the store must
/// support, and what a sync deletes is copied under `.jot/trash/` first.
class S3Transport : public Transport
{
public:
    /// `credentials` is the access key's id and secret, as `<id>:<secret>`.
    S3Transport(S3Location location, const QString &credentials);
    ~S3Transport() override;

    std::optional<QList<RemoteFile>> list(QString *error) override;
    std::optional<QByteArray> get(const QString &path, QString *rev, QString *error) override;
    std::optional<QString> put(const QString &path, const QByteArray &data, const QString &expected,
                               bool *stale, QString *error) override;
    bool remove(const QString &path, const QString &expected, bool *stale, QString *error) override;

    /// Checks the bucket can be listed and refuses a write over an object it was not told of,
    /// as sync relies on; says what is wrong otherwise.
    bool verify(QString *error);

    /// How long a request may take before it counts as failed.
    static constexpr int kTimeoutMs = 60 * 1000;

private:
    struct Reply
    {
        int status = 0;
        QByteArray body;
        QMap<QByteArray, QByteArray> headers;
        QString error;
    };
    Reply send(const QByteArray &method, const QString &key,
               const QList<std::pair<QString, QString>> &query = {}, const QByteArray &body = {},
               const QMap<QByteArray, QByteArray> &extra = {});
    [[nodiscard]] QString keyOf(const QString &path) const;
    /// What a failed reply says went wrong, in words.
    [[nodiscard]] static QString problem(const Reply &reply);

    S3Location m_location;
    QString m_keyId;
    QString m_secret;
    std::unique_ptr<QNetworkAccessManager> m_network;
};

} // namespace jot
