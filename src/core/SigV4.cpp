#include "jot/SigV4.h"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRegularExpression>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

QByteArray hmac(const QByteArray &key, const QByteArray &message)
{
    return QMessageAuthenticationCode::hash(message, key, QCryptographicHash::Sha256);
}

} // namespace

QByteArray awsEncode(const QString &text, bool slashes)
{
    static const QByteArray kept =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";
    QByteArray encoded;
    for (const char byte : text.toUtf8()) {
        if (kept.contains(byte) || (slashes && byte == '/'))
            encoded += byte;
        else
            encoded += '%' + QByteArray::number(static_cast<unsigned char>(byte), 16)
                                 .rightJustified(2, '0')
                                 .toUpper();
    }
    return encoded;
}

QByteArray amzDate(const QDateTime &when)
{
    return when.toUTC().toString(u"yyyyMMdd'T'HHmmss'Z'"_s).toLatin1();
}

QByteArray sigV4Authorization(const SigV4Request &request, const QString &keyId,
                              const QString &secret, const QString &region, const QString &service,
                              const QDateTime &when)
{
    QList<std::pair<QByteArray, QByteArray>> query;
    for (const auto &[name, value] : request.query)
        query.append({awsEncode(name), awsEncode(value)});
    std::ranges::sort(query);
    QByteArrayList pairs;
    for (const auto &[name, value] : std::as_const(query))
        pairs << name + '=' + value;

    // Header values trimmed, with runs of spaces inside made one.
    static const QRegularExpression spaces(u"\\s+"_s);
    QByteArray headers;
    QByteArrayList names;
    for (auto it = request.headers.cbegin(); it != request.headers.cend(); ++it) {
        const QByteArray name = it.key().toLower();
        names << name;
        headers += name + ':' +
                   QString::fromUtf8(it.value()).trimmed().replace(spaces, u" "_s).toUtf8() + '\n';
    }
    const QByteArray signedHeaders = names.join(';');

    const QByteArray canonical = request.method + '\n' + request.path + '\n' + pairs.join('&') +
                                 '\n' + headers + '\n' + signedHeaders + '\n' + request.payloadHash;
    const QByteArray date = amzDate(when);
    const QByteArray day = date.left(8);
    const QByteArray scope = day + '/' + region.toUtf8() + '/' + service.toUtf8() + "/aws4_request";
    const QByteArray toSign =
        "AWS4-HMAC-SHA256\n" + date + '\n' + scope + '\n' +
        QCryptographicHash::hash(canonical, QCryptographicHash::Sha256).toHex();
    QByteArray key = hmac("AWS4" + secret.toUtf8(), day);
    key = hmac(key, region.toUtf8());
    key = hmac(key, service.toUtf8());
    key = hmac(key, "aws4_request");
    return "AWS4-HMAC-SHA256 Credential=" + keyId.toUtf8() + '/' + scope +
           ", SignedHeaders=" + signedHeaders + ", Signature=" + hmac(key, toSign).toHex();
}

} // namespace jot
