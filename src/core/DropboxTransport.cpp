#include "jot/DropboxTransport.h"
#include "jot/SyncBuildConfig.h"

#include <QCryptographicHash>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

constexpr int kTimeoutMs = 60 * 1000;

/// JSON for an HTTP header, which takes ASCII only: anything past it is written `\uXXXX`.
QByteArray headerJson(const QJsonObject &object)
{
    const QString json = QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    QByteArray ascii;
    for (const QChar c : json) {
        if (c.unicode() < 0x80)
            ascii += char(c.unicode());
        else
            ascii += "\\u" + QByteArray::number(c.unicode(), 16).rightJustified(4, '0');
    }
    return ascii;
}

QByteArray base64url(const QByteArray &data)
{
    return data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

} // namespace

QString dropboxAppKey()
{
    return QStringLiteral(JOT_DROPBOX_APP_KEY);
}

DropboxSignIn DropboxSignIn::start(const QString &appKey, const DropboxApi &api)
{
    QByteArray random(48, '\0');
    for (char &byte : random)
        byte = char(QRandomGenerator::system()->bounded(256));
    DropboxSignIn signIn;
    signIn.verifier = QString::fromLatin1(base64url(random));
    QUrl url = api.authorize;
    QUrlQuery query;
    query.addQueryItem(u"client_id"_s, appKey);
    query.addQueryItem(u"response_type"_s, u"code"_s);
    query.addQueryItem(u"token_access_type"_s, u"offline"_s);
    query.addQueryItem(u"code_challenge_method"_s, u"S256"_s);
    query.addQueryItem(u"code_challenge"_s,
                       QString::fromLatin1(base64url(QCryptographicHash::hash(
                           signIn.verifier.toLatin1(), QCryptographicHash::Sha256))));
    url.setQuery(query);
    signIn.url = url;
    return signIn;
}

std::optional<QString> DropboxSignIn::finish(const QString &appKey, const QString &code,
                                             QString *error, const DropboxApi &api) const
{
    QNetworkAccessManager network;
    QUrl url = api.api;
    url.setPath(u"/oauth2/token"_s);
    QNetworkRequest request(url);
    request.setTransferTimeout(kTimeoutMs);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QUrlQuery form;
    form.addQueryItem(u"grant_type"_s, u"authorization_code"_s);
    form.addQueryItem(u"code"_s, code.trimmed());
    form.addQueryItem(u"client_id"_s, appKey);
    form.addQueryItem(u"code_verifier"_s, verifier);
    QNetworkReply *reply = network.post(request, form.query(QUrl::FullyEncoded).toLatin1());
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const QJsonObject answer = QJsonDocument::fromJson(reply->readAll()).object();
    reply->deleteLater();
    const QString token = answer.value(u"refresh_token"_s).toString();
    if (token.isEmpty()) {
        *error = answer.value(u"error_description"_s)
                     .toString(answer.value(u"error"_s)
                                   .toString(QObject::tr("Dropbox did not take the code")));
        return std::nullopt;
    }
    return token;
}

DropboxTransport::DropboxTransport(QString appKey, QString refreshToken, QString folder,
                                   DropboxApi api)
    : m_appKey(std::move(appKey)), m_refreshToken(std::move(refreshToken)),
      m_folder(std::move(folder)), m_api(std::move(api)),
      m_network(std::make_unique<QNetworkAccessManager>())
{
    while (m_folder.endsWith(u'/'))
        m_folder.chop(1);
    if (!m_folder.isEmpty() && !m_folder.startsWith(u'/'))
        m_folder.prepend(u'/');
}

DropboxTransport::~DropboxTransport() = default;

QString DropboxTransport::dropboxPath(const QString &path) const
{
    return m_folder + u'/' + path;
}

DropboxTransport::Reply DropboxTransport::post(const QUrl &url, const QByteArray &body,
                                               const QMap<QByteArray, QByteArray> &headers)
{
    QNetworkRequest request(url);
    request.setTransferTimeout(kTimeoutMs);
    for (auto it = headers.cbegin(); it != headers.cend(); ++it)
        request.setRawHeader(it.key(), it.value());
    QNetworkReply *reply = m_network->post(request, body);
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

bool DropboxTransport::authorize(QString *error)
{
    if (!m_accessToken.isEmpty())
        return true;
    QUrl url = m_api.api;
    url.setPath(u"/oauth2/token"_s);
    QUrlQuery form;
    form.addQueryItem(u"grant_type"_s, u"refresh_token"_s);
    form.addQueryItem(u"refresh_token"_s, m_refreshToken);
    form.addQueryItem(u"client_id"_s, m_appKey);
    const Reply reply = post(url, form.query(QUrl::FullyEncoded).toLatin1(),
                             {{"Content-Type", "application/x-www-form-urlencoded"}});
    m_accessToken =
        QJsonDocument::fromJson(reply.body).object().value(u"access_token"_s).toString();
    if (m_accessToken.isEmpty()) {
        *error = QObject::tr("Dropbox did not let Jot in: %1").arg(problem(reply));
        return false;
    }
    return true;
}

DropboxTransport::Reply DropboxTransport::rpc(const QString &endpoint, const QJsonObject &arguments,
                                              QString *error)
{
    if (!authorize(error))
        return {};
    QUrl url = m_api.api;
    url.setPath(u"/2/"_s + endpoint);
    return post(url, QJsonDocument(arguments).toJson(QJsonDocument::Compact),
                {{"Authorization", "Bearer " + m_accessToken.toUtf8()},
                 {"Content-Type", "application/json"}});
}

DropboxTransport::Reply DropboxTransport::content(const QString &endpoint,
                                                  const QJsonObject &arguments,
                                                  const QByteArray &body, QString *error)
{
    if (!authorize(error))
        return {};
    QUrl url = m_api.content;
    url.setPath(u"/2/"_s + endpoint);
    return post(url, body,
                {{"Authorization", "Bearer " + m_accessToken.toUtf8()},
                 {"Dropbox-API-Arg", headerJson(arguments)},
                 {"Content-Type", "application/octet-stream"}});
}

QString DropboxTransport::problem(const Reply &reply)
{
    if (!reply.error.isEmpty())
        return reply.error;
    const QJsonObject answer = QJsonDocument::fromJson(reply.body).object();
    const QString summary =
        answer.value(u"error_summary"_s).toString(answer.value(u"error_description"_s).toString());
    return summary.isEmpty()
               ? QObject::tr("Dropbox answered %1").arg(reply.status)
               : QObject::tr("Dropbox answered %1: %2").arg(reply.status).arg(summary);
}

std::optional<QList<RemoteFile>> DropboxTransport::list(QString *error)
{
    QList<RemoteFile> files;
    Reply reply =
        rpc(u"files/list_folder"_s,
            {{u"path"_s, m_folder}, {u"recursive"_s, true}, {u"include_deleted"_s, false}}, error);
    while (true) {
        if (reply.status == 409 && reply.body.contains("not_found"))
            return files;
        if (reply.status != 200) {
            if (error->isEmpty())
                *error = problem(reply);
            return std::nullopt;
        }
        const QJsonObject page = QJsonDocument::fromJson(reply.body).object();
        for (const QJsonValue &each : page.value(u"entries"_s).toArray()) {
            const QJsonObject entry = each.toObject();
            if (entry.value(u".tag"_s).toString() != u"file")
                continue;
            const QString path = entry.value(u"path_display"_s).toString().mid(m_folder.size() + 1);
            if (!path.startsWith(u".jot/trash/"_s))
                files.append({path, entry.value(u"rev"_s).toString(), {}});
        }
        if (!page.value(u"has_more"_s).toBool())
            return files;
        reply = rpc(u"files/list_folder/continue"_s,
                    {{u"cursor"_s, page.value(u"cursor"_s).toString()}}, error);
    }
}

std::optional<QByteArray> DropboxTransport::get(const QString &path, QString *rev, QString *error)
{
    const Reply reply = content(u"files/download"_s, {{u"path"_s, dropboxPath(path)}}, {}, error);
    if (reply.status != 200) {
        if (error->isEmpty())
            *error = problem(reply);
        return std::nullopt;
    }
    *rev = QJsonDocument::fromJson(reply.headers.value("dropbox-api-result"))
               .object()
               .value(u"rev"_s)
               .toString();
    return reply.body;
}

std::optional<QString> DropboxTransport::put(const QString &path, const QByteArray &data,
                                             const QString &expected, bool *stale, QString *error)
{
    if (data.size() > kMaxUpload) {
        *error = QObject::tr("it is over 150 MB, more than Jot sends to Dropbox in one piece");
        return std::nullopt;
    }
    // Added where there is nothing, or put over the revision expected; else Dropbox refuses.
    const QJsonValue mode =
        expected.isEmpty()
            ? QJsonValue(u"add"_s)
            : QJsonValue(QJsonObject{{u".tag"_s, u"update"_s}, {u"update"_s, expected}});
    const Reply reply = content(u"files/upload"_s,
                                {{u"path"_s, dropboxPath(path)},
                                 {u"mode"_s, mode},
                                 {u"autorename"_s, false},
                                 {u"strict_conflict"_s, true},
                                 {u"mute"_s, true}},
                                data, error);
    if (reply.status == 409 && reply.body.contains("conflict")) {
        *stale = true;
        return std::nullopt;
    }
    if (reply.status != 200) {
        if (error->isEmpty())
            *error = problem(reply);
        return std::nullopt;
    }
    return QJsonDocument::fromJson(reply.body).object().value(u"rev"_s).toString();
}

bool DropboxTransport::remove(const QString &path, const QString &expected, bool *stale,
                              QString *error)
{
    const Reply reply = rpc(u"files/delete_v2"_s,
                            {{u"path"_s, dropboxPath(path)}, {u"parent_rev"_s, expected}}, error);
    if (reply.status == 409) {
        // Gone already, or changed since: either way not what was meant to be deleted.
        *stale = true;
        return false;
    }
    if (reply.status != 200) {
        if (error->isEmpty())
            *error = problem(reply);
        return false;
    }
    return true;
}

} // namespace jot
