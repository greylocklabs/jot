#include "jot/WebVideo.h"

#include <QRegularExpression>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// Seconds from a start time as YouTube writes it: "90", "90s" or "1m30s".
int seconds(const QString &time)
{
    static const QRegularExpression parts(uR"(^(?:(\d+)h)?(?:(\d+)m)?(?:(\d+)s?)?$)"_s);
    const QRegularExpressionMatch m = parts.match(time);
    if (!m.hasMatch() || time.isEmpty())
        return 0;
    return m.captured(1).toInt() * 3600 + m.captured(2).toInt() * 60 + m.captured(3).toInt();
}

} // namespace

std::optional<WebVideo> webVideo(const QString &url)
{
    const QUrl link(url.trimmed(), QUrl::StrictMode);
    if (!link.isValid() || (link.scheme() != u"https" && link.scheme() != u"http"))
        return std::nullopt;
    QString host = link.host().toLower();
    if (host.startsWith(u"www."_s))
        host = host.mid(4);
    else if (host.startsWith(u"m."_s))
        host = host.mid(2);
    const QStringList path = link.path().split(u'/', Qt::SkipEmptyParts);
    const QUrlQuery query(link);

    static const QRegularExpression youtubeId(u"^[A-Za-z0-9_-]{11}$"_s);
    QString id;
    if (host == u"youtube.com" || host == u"music.youtube.com" || host == u"youtube-nocookie.com") {
        if (path.value(0) == u"watch")
            id = query.queryItemValue(u"v"_s);
        else if (path.value(0) == u"shorts" || path.value(0) == u"embed" ||
                 path.value(0) == u"live")
            id = path.value(1);
    } else if (host == u"youtu.be") {
        id = path.value(0);
    }
    if (youtubeId.match(id).hasMatch()) {
        QUrl embed(u"https://www.youtube-nocookie.com/embed/"_s + id);
        const QString time = query.hasQueryItem(u"t"_s) ? query.queryItemValue(u"t"_s)
                                                        : query.queryItemValue(u"start"_s);
        if (const int start = seconds(time); start > 0)
            embed.setQuery(u"start="_s + QString::number(start));
        return WebVideo{u"YouTube"_s, link, embed};
    }

    static const QRegularExpression number(u"^\\d+$"_s);
    QString video;
    QString hash;
    if (host == u"vimeo.com" && number.match(path.value(0)).hasMatch()) {
        video = path.value(0);
        // An unlisted video's link carries its hash after its number.
        hash = path.value(1);
    } else if (host == u"player.vimeo.com" && path.value(0) == u"video" &&
               number.match(path.value(1)).hasMatch()) {
        video = path.value(1);
        hash = query.queryItemValue(u"h"_s);
    }
    if (!video.isEmpty()) {
        static const QRegularExpression hex(u"^[0-9a-f]+$"_s);
        QUrlQuery embedQuery;
        if (hex.match(hash).hasMatch())
            embedQuery.addQueryItem(u"h"_s, hash);
        // Vimeo is asked not to track who watches.
        embedQuery.addQueryItem(u"dnt"_s, u"1"_s);
        QUrl embed(u"https://player.vimeo.com/video/"_s + video);
        embed.setQuery(embedQuery);
        return WebVideo{u"Vimeo"_s, link, embed};
    }
    return std::nullopt;
}

} // namespace jot
