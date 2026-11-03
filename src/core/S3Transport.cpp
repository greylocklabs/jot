#include "jot/S3Transport.h"

#include "jot/SigV4.h"

#include <QCryptographicHash>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>
#include <QXmlStreamReader>

using namespace Qt::StringLiterals;

namespace jot {

std::optional<S3Location> S3Location::parse(const QString &remote)
{
    if (!remote.startsWith(u"s3:"_s))
        return std::nullopt;
    const QUrl url(remote.mid(3));
    const QStringList parts = url.path().split(u'/', Qt::SkipEmptyParts);
    const QUrlQuery query(url);
    if (!url.isValid() || url.host().isEmpty() || parts.isEmpty() ||
        !query.hasQueryItem(u"region"_s))
        return std::nullopt;
    S3Location location;
    location.endpoint = url.adjusted(QUrl::RemovePath | QUrl::RemoveQuery);
    location.region = query.queryItemValue(u"region"_s);
    location.bucket = parts.first();
    location.prefix = parts.mid(1).join(u'/');
    location.pathStyle = query.queryItemValue(u"style"_s) != u"virtual";
    return location;
}

QString S3Location::remote() const
{
    QString path = u'/' + bucket;
    if (!prefix.isEmpty())
        path += u'/' + prefix;
    return u"s3:"_s + endpoint.toString(QUrl::RemovePath) + path + u"?region="_s + region +
           (pathStyle ? QString() : u"&style=virtual"_s);
}

S3Transport::S3Transport(S3Location location, const QString &credentials)
    : m_location(std::move(location)), m_keyId(credentials.section(u':', 0, 0)),
      m_secret(credentials.section(u':', 1)), m_network(std::make_unique<QNetworkAccessManager>())
{}

S3Transport::~S3Transport() = default;

QString S3Transport::keyOf(const QString &path) const
{
    return m_location.prefix.isEmpty() ? path : m_location.prefix + u'/' + path;
}

S3Transport::Reply S3Transport::send(const QByteArray &method, const QString &key,
                                     const QList<std::pair<QString, QString>> &query,
                                     const QByteArray &body,
                                     const QMap<QByteArray, QByteArray> &extra)
{
    const QUrl &endpoint = m_location.endpoint;
    QString host = endpoint.host();
    QByteArray path = '/' + awsEncode(key, true);
    if (m_location.pathStyle)
        path = '/' + awsEncode(m_location.bucket) + (key.isEmpty() ? QByteArray() : path);
    else
        host = m_location.bucket + u'.' + host;
    if (endpoint.port() != -1)
        host += u':' + QString::number(endpoint.port());

    QList<std::pair<QByteArray, QByteArray>> encoded;
    for (const auto &[name, value] : query)
        encoded.append({awsEncode(name), awsEncode(value)});
    std::ranges::sort(encoded);
    QByteArrayList pairs;
    for (const auto &[name, value] : std::as_const(encoded))
        pairs << name + '=' + value;

    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QByteArray payloadHash =
        QCryptographicHash::hash(body, QCryptographicHash::Sha256).toHex();
    SigV4Request signing{method,
                         path,
                         query,
                         {{"host", host.toUtf8()},
                          {"x-amz-content-sha256", payloadHash},
                          {"x-amz-date", amzDate(now)}},
                         payloadHash};
    for (auto it = extra.cbegin(); it != extra.cend(); ++it) {
        if (it.key().startsWith("x-amz-"))
            signing.headers.insert(it.key(), it.value());
    }

    const QUrl url = QUrl::fromEncoded(endpoint.scheme().toUtf8() + "://" + host.toUtf8() + path +
                                           (pairs.isEmpty() ? QByteArray() : '?' + pairs.join('&')),
                                       QUrl::StrictMode);
    QNetworkRequest request(url);
    request.setTransferTimeout(kTimeoutMs);
    for (auto it = signing.headers.cbegin(); it != signing.headers.cend(); ++it) {
        if (it.key() != "host")
            request.setRawHeader(it.key(), it.value());
    }
    for (auto it = extra.cbegin(); it != extra.cend(); ++it)
        request.setRawHeader(it.key(), it.value());
    request.setRawHeader("Authorization", sigV4Authorization(signing, m_keyId, m_secret,
                                                             m_location.region, u"s3"_s, now));
    if (method == "PUT")
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");

    QNetworkReply *reply = m_network->sendCustomRequest(request, method, body);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    Reply result;
    result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.body = reply->readAll();
    for (const auto &[name, value] : reply->rawHeaderPairs())
        result.headers.insert(name.toLower(), value);
    if (result.status == 0)
        result.error = reply->errorString();
    reply->deleteLater();
    return result;
}

QString S3Transport::problem(const Reply &reply)
{
    if (!reply.error.isEmpty())
        return reply.error;
    // S3 says what went wrong in an XML Error: its Message, or else its Code.
    QXmlStreamReader xml(reply.body);
    QString code;
    QString message;
    while (!xml.atEnd()) {
        if (xml.readNext() != QXmlStreamReader::StartElement)
            continue;
        if (xml.name() == u"Code")
            code = xml.readElementText();
        else if (xml.name() == u"Message")
            message = xml.readElementText();
    }
    const QString said = !message.isEmpty() ? message : code;
    return said.isEmpty() ? QObject::tr("the store answered %1").arg(reply.status)
                          : QObject::tr("the store answered %1: %2").arg(reply.status).arg(said);
}

std::optional<QList<RemoteFile>> S3Transport::list(QString *error)
{
    QList<RemoteFile> files;
    const QString prefix = m_location.prefix.isEmpty() ? QString() : m_location.prefix + u'/';
    QString token;
    do {
        QList<std::pair<QString, QString>> query = {{u"list-type"_s, u"2"_s},
                                                    {u"prefix"_s, prefix}};
        if (!token.isEmpty())
            query.append({u"continuation-token"_s, token});
        const Reply reply = send("GET", {}, query);
        if (reply.status != 200) {
            *error = problem(reply);
            return std::nullopt;
        }
        token.clear();
        bool truncated = false;
        QXmlStreamReader xml(reply.body);
        QString key;
        QString etag;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()) {
                if (xml.name() == u"Contents") {
                    key.clear();
                    etag.clear();
                } else if (xml.name() == u"Key") {
                    key = xml.readElementText();
                } else if (xml.name() == u"ETag") {
                    etag = xml.readElementText();
                } else if (xml.name() == u"IsTruncated") {
                    truncated = xml.readElementText() == u"true";
                } else if (xml.name() == u"NextContinuationToken") {
                    token = xml.readElementText();
                }
            } else if (xml.isEndElement() && xml.name() == u"Contents") {
                const QString path = key.mid(prefix.size());
                // What a sync deleted, and folder markers, are not the vault's files.
                if (!path.isEmpty() && !path.endsWith(u'/') && !path.startsWith(u".jot/trash/"_s))
                    files.append({path, etag, {}});
            }
        }
        if (xml.hasError()) {
            *error = QObject::tr("could not read the bucket's listing: %1").arg(xml.errorString());
            return std::nullopt;
        }
        if (!truncated)
            token.clear();
    } while (!token.isEmpty());
    return files;
}

std::optional<QByteArray> S3Transport::get(const QString &path, QString *rev, QString *error)
{
    const Reply reply = send("GET", keyOf(path));
    if (reply.status != 200) {
        *error = problem(reply);
        return std::nullopt;
    }
    *rev = QString::fromUtf8(reply.headers.value("etag"));
    return reply.body;
}

std::optional<QString> S3Transport::put(const QString &path, const QByteArray &data,
                                        const QString &expected, bool *stale, QString *error)
{
    // Written only over the version expected, or only where there is none.
    const QMap<QByteArray, QByteArray> condition =
        expected.isEmpty() ? QMap<QByteArray, QByteArray>{{"If-None-Match", "*"}}
                           : QMap<QByteArray, QByteArray>{{"If-Match", expected.toUtf8()}};
    const Reply reply = send("PUT", keyOf(path), {}, data, condition);
    if (reply.status == 412 || reply.status == 409) {
        *stale = true;
        return std::nullopt;
    }
    if (reply.status != 200) {
        *error = problem(reply);
        return std::nullopt;
    }
    return QString::fromUtf8(reply.headers.value("etag"));
}

bool S3Transport::remove(const QString &path, const QString &expected, bool *stale, QString *error)
{
    const Reply head = send("HEAD", keyOf(path));
    if (head.status == 404 ||
        (head.status == 200 && QString::fromUtf8(head.headers.value("etag")) != expected)) {
        *stale = true;
        return false;
    }
    if (head.status != 200) {
        *error = problem(head);
        return false;
    }
    // Kept under .jot/trash/ first, as the folder remote does, copying only the version expected;
    // one written since is no longer what was meant to be deleted.
    const QString trashed = u".jot/trash/"_s +
                            QDateTime::currentDateTime().toString(u"yyyy-MM-dd HHmmss"_s) + u'/' +
                            path;
    const QByteArray source =
        '/' + awsEncode(m_location.bucket) + '/' + awsEncode(keyOf(path), true);
    const Reply copy =
        send("PUT", keyOf(trashed), {}, {},
             {{"x-amz-copy-source", source}, {"x-amz-copy-source-if-match", expected.toUtf8()}});
    if (copy.status == 412) {
        *stale = true;
        return false;
    }
    // A copy can fail with 200 and an Error in its body; then there is no copy to fall back on.
    if (copy.status != 200 || !copy.body.contains("CopyObjectResult")) {
        *error = problem(copy);
        return false;
    }
    // Deleted only if still that version where the store checks, as it is where it does not.
    Reply gone = send("DELETE", keyOf(path), {}, {}, {{"If-Match", expected.toUtf8()}});
    // A store that does not take If-Match on a delete says so with 501; anything else stands.
    if (gone.status == 501)
        gone = send("DELETE", keyOf(path));
    if (gone.status == 412) {
        *stale = true;
        return false;
    }
    if (gone.status != 204 && gone.status != 200) {
        *error = problem(gone);
        return false;
    }
    return true;
}

bool S3Transport::verify(QString *error)
{
    if (!list(error))
        return false;
    // A store that writes over an object it was told is not there would let two computers
    // overwrite each other's edits unseen.
    const QString probe = u".jot-probe-"_s + QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool stale = false;
    const std::optional<QString> rev = put(probe, "probe\n", {}, &stale, error);
    if (!rev)
        return false;
    // Nor over one it was told of but which has changed since; and over the one it holds, yes.
    bool overNew = false;
    bool overOld = false;
    const bool refusedNew = !put(probe, "again\n", {}, &overNew, error) && overNew;
    const bool refusedOld = !put(probe, "again\n", u"\"0\""_s, &overOld, error) && overOld;
    const std::optional<QString> over = put(probe, "again\n", *rev, &stale, error);
    send("DELETE", keyOf(probe));
    if (!refusedNew || !refusedOld || !over) {
        *error = QObject::tr("this store does not support conditional writes, which sync needs "
                             "so two computers never overwrite each other");
        return false;
    }
    error->clear();
    return true;
}

} // namespace jot
