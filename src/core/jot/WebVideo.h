#pragma once

#include <QString>
#include <QUrl>

#include <optional>

namespace jot {

/// A video on a site whose player can sit in a note.
struct WebVideo
{
    /// The site's name, as the card says it: YouTube or Vimeo.
    QString site;
    /// The link as written in the note.
    QUrl link;
    /// The site's player for it, from youtube-nocookie.com for YouTube.
    QUrl embed;

    bool operator==(const WebVideo &) const = default;
};

/// The web video at `url`: a YouTube watch, youtu.be, Shorts or embed link, or a Vimeo link,
/// keeping a start time and an unlisted Vimeo video's hash. Nothing for any other link.
[[nodiscard]] std::optional<WebVideo> webVideo(const QString &url);

} // namespace jot
