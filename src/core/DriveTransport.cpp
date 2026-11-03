#include "jot/DriveTransport.h"
#include "jot/SyncBuildConfig.h"

#include <QCryptographicHash>
#include <QDeadlineTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

constexpr int kTimeoutMs = 60 * 1000;

QByteArray randomText(int bytes)
{
    QByteArray random(bytes, '\0');
    for (char &byte : random)
        byte = char(QRandomGenerator::system()->bounded(256));
    return random.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

/// A string for Drive's query language, its quotes and backslashes escaped.
QString quoted(QString text)
{
    text.replace(u"\\"_s, u"\\\\"_s);
    text.replace(u"'"_s, u"\\'"_s);
    return u"'"_s + text + u"'"_s;
}

/// Posts a form and gives what came back as JSON.
QJsonObject postForm(const QUrl &url, const QUrlQuery &form, QString *error)
{
    QNetworkAccessManager network;
    QNetworkRequest request(url);
    request.setTransferTimeout(kTimeoutMs);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QNetworkReply *reply = network.post(request, form.query(QUrl::FullyEncoded).toLatin1());
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const QJsonObject answer = QJsonDocument::fromJson(reply->readAll()).object();
    if (answer.isEmpty())
        *error = reply->errorString();
    else if (answer.contains(u"error"_s))
        *error = answer.value(u"error_description"_s).toString(answer.value(u"error"_s).toString());
    reply->deleteLater();
    return answer;
}

} // namespace

QString googleClientId()
{
    return QStringLiteral(JOT_GOOGLE_CLIENT_ID);
}

QString googleClientSecret()
{
    return QStringLiteral(JOT_GOOGLE_CLIENT_SECRET);
}

DriveSignIn::DriveSignIn(DriveApi api) : m_api(std::move(api)) {}

DriveSignIn::~DriveSignIn() = default;

QUrl DriveSignIn::start(const QString &clientId)
{
    m_server = std::make_unique<QTcpServer>();
    if (!m_server->listen(QHostAddress::LocalHost, 0))
        return {};
    m_verifier = QString::fromLatin1(randomText(48));
    m_state = QString::fromLatin1(randomText(16));
    m_redirect = u"http://127.0.0.1:%1"_s.arg(m_server->serverPort());
    QUrlQuery query;
    query.addQueryItem(u"client_id"_s, clientId);
    query.addQueryItem(u"redirect_uri"_s, m_redirect);
    query.addQueryItem(u"response_type"_s, u"code"_s);
    query.addQueryItem(u"scope"_s, u"https://www.googleapis.com/auth/drive.file"_s);
    query.addQueryItem(u"access_type"_s, u"offline"_s);
    query.addQueryItem(u"prompt"_s, u"consent"_s);
    query.addQueryItem(u"state"_s, m_state);
    query.addQueryItem(u"code_challenge_method"_s, u"S256"_s);
    query.addQueryItem(
        u"code_challenge"_s,
        QString::fromLatin1(
            QCryptographicHash::hash(m_verifier.toLatin1(), QCryptographicHash::Sha256)
                .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)));
    QUrl url = m_api.authorize;
    url.setQuery(query);
    return url;
}

std::optional<QString> DriveSignIn::finish(const QString &clientId, const QString &clientSecret,
                                           int timeoutMs, QString *error,
                                           const std::atomic<bool> *cancel)
{
    if (!m_server) {
        *error = QObject::tr("the sign-in was not started");
        return std::nullopt;
    }
    // The browser's request for the redirect, read whole: its first line holds the code.
    QString code;
    QString refused;
    QEventLoop loop;
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    // Looked at now and then, so quitting the app need not wait for the browser.
    QTimer watch;
    QObject::connect(&watch, &QTimer::timeout, &loop, [&] {
        if (cancel && cancel->load())
            loop.quit();
    });
    watch.start(100);
    QObject::connect(m_server.get(), &QTcpServer::newConnection, &loop, [&] {
        QTcpSocket *socket = m_server->nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::readyRead, &loop, [&, socket] {
            const QByteArray head = socket->readAll();
            const QList<QByteArray> line = head.left(head.indexOf('\r')).split(' ');
            const QUrlQuery query(QUrl(QString::fromLatin1(line.value(1))));
            if (query.queryItemValue(u"state"_s) != m_state) {
                socket->write("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n");
                socket->disconnectFromHost();
                return;
            }
            code = query.queryItemValue(u"code"_s);
            refused = query.queryItemValue(u"error"_s);
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                          "Connection: close\r\n\r\n<!doctype html><title>Jot</title>"
                          "<p>Jot has what it needs. You can close this tab.</p>");
            socket->disconnectFromHost();
            loop.quit();
        });
    });
    loop.exec();
    m_server.reset();
    if (code.isEmpty()) {
        *error = refused.isEmpty() ? QObject::tr("the browser did not come back in time")
                                   : QObject::tr("Google said: %1").arg(refused);
        return std::nullopt;
    }
    QUrlQuery form;
    form.addQueryItem(u"grant_type"_s, u"authorization_code"_s);
    form.addQueryItem(u"code"_s, code);
    form.addQueryItem(u"client_id"_s, clientId);
    form.addQueryItem(u"client_secret"_s, clientSecret);
    form.addQueryItem(u"redirect_uri"_s, m_redirect);
    form.addQueryItem(u"code_verifier"_s, m_verifier);
    const QJsonObject answer = postForm(m_api.token, form, error);
    const QString token = answer.value(u"refresh_token"_s).toString();
    if (token.isEmpty()) {
        if (error->isEmpty())
            *error = QObject::tr("Google gave no lasting sign-in");
        return std::nullopt;
    }
    return token;
}

DriveTransport::DriveTransport(QString clientId, QString clientSecret, QString refreshToken,
                               QString folder, DriveApi api)
    : m_clientId(std::move(clientId)), m_clientSecret(std::move(clientSecret)),
      m_refreshToken(std::move(refreshToken)), m_folder(std::move(folder)), m_api(std::move(api)),
      m_network(std::make_unique<QNetworkAccessManager>())
{}

DriveTransport::~DriveTransport() = default;

QUrl DriveTransport::apiUrl(const QString &path,
                            const QList<std::pair<QString, QString>> &query) const
{
    QUrl url = m_api.api;
    url.setPath(path);
    QByteArrayList items;
    for (const auto &[name, value] : query)
        items << QUrl::toPercentEncoding(name) + '=' + QUrl::toPercentEncoding(value);
    if (!items.isEmpty())
        url.setQuery(QString::fromLatin1(items.join('&')), QUrl::StrictMode);
    return url;
}

bool DriveTransport::authorize(QString *error)
{
    if (!m_accessToken.isEmpty())
        return true;
    QUrlQuery form;
    form.addQueryItem(u"grant_type"_s, u"refresh_token"_s);
    form.addQueryItem(u"refresh_token"_s, m_refreshToken);
    form.addQueryItem(u"client_id"_s, m_clientId);
    form.addQueryItem(u"client_secret"_s, m_clientSecret);
    QString refused;
    m_accessToken = postForm(m_api.token, form, &refused).value(u"access_token"_s).toString();
    if (m_accessToken.isEmpty()) {
        *error = QObject::tr("Google did not let Jot in: %1").arg(refused);
        return false;
    }
    return true;
}

DriveTransport::Reply DriveTransport::call(const QByteArray &method, const QUrl &url,
                                           const QByteArray &body, const QByteArray &contentType)
{
    QNetworkRequest request(url);
    request.setTransferTimeout(kTimeoutMs);
    request.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
    if (!contentType.isEmpty())
        request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    QNetworkReply *reply = m_network->sendCustomRequest(request, method, body);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    Reply result;
    result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.body = reply->readAll();
    if (result.status == 0)
        result.error = reply->errorString();
    reply->deleteLater();
    return result;
}

QString DriveTransport::problem(const Reply &reply)
{
    if (!reply.error.isEmpty())
        return reply.error;
    const QString message = QJsonDocument::fromJson(reply.body)
                                .object()
                                .value(u"error"_s)
                                .toObject()
                                .value(u"message"_s)
                                .toString();
    return message.isEmpty() ? QObject::tr("Google answered %1").arg(reply.status)
                             : QObject::tr("Google answered %1: %2").arg(reply.status).arg(message);
}

std::optional<QList<RemoteFile>> DriveTransport::list(QString *error)
{
    if (!authorize(error))
        return std::nullopt;
    m_files.clear();
    m_folders = {{QString(), m_folder}};
    // Folder by folder from the vault's own, as Drive lists only one folder's children at once.
    QList<std::pair<QString, QString>> folders = {{QString(), m_folder}};
    while (!folders.isEmpty()) {
        const auto [path, id] = folders.takeFirst();
        QString page;
        do {
            QList<std::pair<QString, QString>> query = {
                {u"q"_s, quoted(id) + u" in parents and trashed = false"_s},
                {u"fields"_s, u"nextPageToken,files(id,name,mimeType,version)"_s},
                {u"pageSize"_s, u"1000"_s}};
            if (!page.isEmpty())
                query.append({u"pageToken"_s, page});
            const Reply reply = call("GET", apiUrl(u"/drive/v3/files"_s, query));
            if (reply.status != 200) {
                *error = problem(reply);
                return std::nullopt;
            }
            const QJsonObject answer = QJsonDocument::fromJson(reply.body).object();
            for (const QJsonValue &each : answer.value(u"files"_s).toArray()) {
                const QJsonObject file = each.toObject();
                const QString name = file.value(u"name"_s).toString();
                const QString at = path.isEmpty() ? name : path + u'/' + name;
                // Two of one name in one folder, which Drive allows, are taken as the first.
                if (file.value(u"mimeType"_s).toString() == QLatin1StringView(kFolderType)) {
                    if (!m_folders.contains(at)) {
                        m_folders.insert(at, file.value(u"id"_s).toString());
                        folders.append({at, file.value(u"id"_s).toString()});
                    }
                } else if (!m_files.contains(at)) {
                    m_files.insert(at, {file.value(u"id"_s).toString(),
                                        file.value(u"version"_s).toVariant().toString()});
                }
            }
            page = answer.value(u"nextPageToken"_s).toString();
        } while (!page.isEmpty());
    }
    m_listed = true;
    QList<RemoteFile> files;
    for (auto it = m_files.cbegin(); it != m_files.cend(); ++it)
        files.append({it.key(), it->version, {}});
    return files;
}

bool DriveTransport::known(QString *error)
{
    return m_listed || list(error).has_value();
}

std::optional<QString> DriveTransport::versionOf(const QString &id, QString *error)
{
    const Reply reply =
        call("GET", apiUrl(u"/drive/v3/files/"_s + id, {{u"fields"_s, u"version,trashed"_s}}));
    if (reply.status == 404)
        return QString();
    if (reply.status != 200) {
        *error = problem(reply);
        return std::nullopt;
    }
    const QJsonObject file = QJsonDocument::fromJson(reply.body).object();
    return file.value(u"trashed"_s).toBool() ? QString()
                                             : file.value(u"version"_s).toVariant().toString();
}

std::optional<QString> DriveTransport::childNamed(const QString &parent, const QString &name,
                                                  bool folder, QString *error)
{
    const QString type = u"mimeType "_s + (folder ? u"="_s : u"!="_s) + u" '"_s +
                         QLatin1StringView(kFolderType) + u"'"_s;
    const Reply reply =
        call("GET", apiUrl(u"/drive/v3/files"_s,
                           {{u"q"_s, u"name = "_s + quoted(name) + u" and "_s + quoted(parent) +
                                         u" in parents and "_s + type + u" and trashed = false"_s},
                            {u"fields"_s, u"files(id)"_s}}));
    if (reply.status != 200) {
        *error = problem(reply);
        return std::nullopt;
    }
    const QJsonArray files =
        QJsonDocument::fromJson(reply.body).object().value(u"files"_s).toArray();
    return files.isEmpty() ? QString() : files.first().toObject().value(u"id"_s).toString();
}

std::optional<QString> DriveTransport::folderFor(const QString &path, QString *error)
{
    const QString folder = path.contains(u'/') ? path.section(u'/', 0, -2) : QString();
    if (const auto found = m_folders.constFind(folder); found != m_folders.cend())
        return *found;
    const std::optional<QString> parent = folderFor(folder, error);
    if (!parent)
        return std::nullopt;
    // Another computer may have made it since the listing; two of one name would split it.
    const std::optional<QString> there = childNamed(*parent, folder.section(u'/', -1), true, error);
    if (!there)
        return std::nullopt;
    if (!there->isEmpty()) {
        m_folders.insert(folder, *there);
        return *there;
    }
    const QJsonObject metadata{{u"name"_s, folder.section(u'/', -1)},
                               {u"mimeType"_s, QLatin1StringView(kFolderType)},
                               {u"parents"_s, QJsonArray{*parent}}};
    const Reply reply =
        call("POST", apiUrl(u"/drive/v3/files"_s, {{u"fields"_s, u"id"_s}}),
             QJsonDocument(metadata).toJson(QJsonDocument::Compact), "application/json");
    if (reply.status != 200) {
        *error = problem(reply);
        return std::nullopt;
    }
    const QString id = QJsonDocument::fromJson(reply.body).object().value(u"id"_s).toString();
    m_folders.insert(folder, id);
    return id;
}

std::optional<QByteArray> DriveTransport::get(const QString &path, QString *rev, QString *error)
{
    if (!authorize(error) || !known(error))
        return std::nullopt;
    const auto found = m_files.constFind(path);
    if (found == m_files.cend()) {
        *error = QObject::tr("it is not in Google Drive");
        return std::nullopt;
    }
    const std::optional<QString> version = versionOf(found->id, error);
    if (!version)
        return std::nullopt;
    const Reply reply =
        call("GET", apiUrl(u"/drive/v3/files/"_s + found->id, {{u"alt"_s, u"media"_s}}));
    if (reply.status != 200) {
        *error = problem(reply);
        return std::nullopt;
    }
    *rev = *version;
    return reply.body;
}

std::optional<QString> DriveTransport::put(const QString &path, const QByteArray &data,
                                           const QString &expected, bool *stale, QString *error)
{
    if (!authorize(error) || !known(error))
        return std::nullopt;
    const auto found = m_files.constFind(path);
    // As near to conditional as Drive allows: the version there, read just before writing.
    if (found != m_files.cend()) {
        const std::optional<QString> version = versionOf(found->id, error);
        if (!version)
            return std::nullopt;
        if (*version != expected) {
            *stale = true;
            return std::nullopt;
        }
    } else if (!expected.isEmpty()) {
        *stale = true;
        return std::nullopt;
    }
    Reply reply;
    QString id;
    if (found != m_files.cend() && !expected.isEmpty()) {
        id = found->id;
        reply = call("PATCH",
                     apiUrl(u"/upload/drive/v3/files/"_s + id,
                            {{u"uploadType"_s, u"media"_s}, {u"fields"_s, u"id,version"_s}}),
                     data, "application/octet-stream");
    } else {
        const std::optional<QString> parent = folderFor(path, error);
        if (!parent)
            return std::nullopt;
        // Nothing there at the listing does not mean nothing there now: another computer may
        // have made it since, and Drive would keep both under one name.
        const std::optional<QString> there =
            childNamed(*parent, path.section(u'/', -1), false, error);
        if (!there)
            return std::nullopt;
        if (!there->isEmpty()) {
            *stale = true;
            return std::nullopt;
        }
        // A new file: its name and folder, then its bytes, in one multipart request.
        const QByteArray boundary = "jot-" + randomText(12);
        const QJsonObject metadata{{u"name"_s, path.section(u'/', -1)},
                                   {u"parents"_s, QJsonArray{*parent}}};
        const QByteArray body = "--" + boundary +
                                "\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n" +
                                QJsonDocument(metadata).toJson(QJsonDocument::Compact) + "\r\n--" +
                                boundary + "\r\nContent-Type: application/octet-stream\r\n\r\n" +
                                data + "\r\n--" + boundary + "--\r\n";
        reply = call("POST",
                     apiUrl(u"/upload/drive/v3/files"_s,
                            {{u"uploadType"_s, u"multipart"_s}, {u"fields"_s, u"id,version"_s}}),
                     body, "multipart/related; boundary=" + boundary);
    }
    if (reply.status != 200) {
        *error = problem(reply);
        return std::nullopt;
    }
    const QJsonObject file = QJsonDocument::fromJson(reply.body).object();
    const QString version = file.value(u"version"_s).toVariant().toString();
    m_files.insert(path, {file.value(u"id"_s).toString(id), version});
    return version;
}

bool DriveTransport::remove(const QString &path, const QString &expected, bool *stale,
                            QString *error)
{
    if (!authorize(error) || !known(error))
        return false;
    const auto found = m_files.constFind(path);
    if (found == m_files.cend()) {
        *stale = true;
        return false;
    }
    const std::optional<QString> version = versionOf(found->id, error);
    if (!version)
        return false;
    if (*version != expected) {
        *stale = true;
        return false;
    }
    // To Drive's trash, where it stays for a while.
    const Reply reply =
        call("PATCH", apiUrl(u"/drive/v3/files/"_s + found->id),
             QJsonDocument(QJsonObject{{u"trashed"_s, true}}).toJson(QJsonDocument::Compact),
             "application/json");
    if (reply.status != 200) {
        *error = problem(reply);
        return false;
    }
    m_files.remove(path);
    return true;
}

std::optional<QString> DriveTransport::folderNamed(const QString &name, QString *error)
{
    if (!authorize(error))
        return std::nullopt;
    const Reply found =
        call("GET", apiUrl(u"/drive/v3/files"_s,
                           {{u"q"_s, u"name = "_s + quoted(name) + u" and mimeType = '"_s +
                                         QLatin1StringView(kFolderType) +
                                         u"' and 'root' in parents and trashed = false"_s},
                            {u"fields"_s, u"files(id)"_s}}));
    if (found.status != 200) {
        *error = problem(found);
        return std::nullopt;
    }
    const QJsonArray files =
        QJsonDocument::fromJson(found.body).object().value(u"files"_s).toArray();
    if (!files.isEmpty())
        return files.first().toObject().value(u"id"_s).toString();
    const QJsonObject metadata{{u"name"_s, name},
                               {u"mimeType"_s, QLatin1StringView(kFolderType)},
                               {u"parents"_s, QJsonArray{u"root"_s}}};
    const Reply made =
        call("POST", apiUrl(u"/drive/v3/files"_s, {{u"fields"_s, u"id"_s}}),
             QJsonDocument(metadata).toJson(QJsonDocument::Compact), "application/json");
    if (made.status != 200) {
        *error = problem(made);
        return std::nullopt;
    }
    return QJsonDocument::fromJson(made.body).object().value(u"id"_s).toString();
}

} // namespace jot
