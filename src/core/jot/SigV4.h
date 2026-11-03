#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>

namespace jot {

/// A request to sign with AWS Signature Version 4, as S3 and the services that copy it take.
struct SigV4Request
{
    QByteArray method;
    /// The path, already encoded as awsEncode() does it.
    QByteArray path;
    /// The query's names and values, not yet encoded.
    QList<std::pair<QString, QString>> query;
    /// The headers to sign, by lowercase name; `host` and `x-amz-date` among them.
    QMap<QByteArray, QByteArray> headers;
    /// The SHA-256 of the body, in hex, or `UNSIGNED-PAYLOAD`.
    QByteArray payloadHash;
};

/// `text` percent-encoded as AWS wants it: every byte but letters, digits and `-_.~`, and `/`
/// too unless `slashes` keeps them.
[[nodiscard]] QByteArray awsEncode(const QString &text, bool slashes = false);

/// The time as `x-amz-date` writes it, such as `20150830T123600Z`.
[[nodiscard]] QByteArray amzDate(const QDateTime &when);

/// The `Authorization` header that signs `request` with the access key `keyId` and its secret,
/// for `region` and `service` at `when`.
[[nodiscard]] QByteArray sigV4Authorization(const SigV4Request &request, const QString &keyId,
                                            const QString &secret, const QString &region,
                                            const QString &service, const QDateTime &when);

} // namespace jot
