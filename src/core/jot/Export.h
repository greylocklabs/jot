#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QStringView>

#include <functional>

namespace jot {

/// A file type Pandoc writes a note out as.
struct ExportFormat
{
    QString id;
    QString label;
    QString extension;
};

/// DOCX, ODT, PDF, ePub and HTML.
[[nodiscard]] QList<ExportFormat> exportFormats();

/// A note's text as Pandoc should read it: front matter left out, wikilinks as their labels,
/// and CriticMarkup with its edits made and its comments left out. Pictures, `![[embeds]]` and
/// `![words](path)` alike, are at the path in the vault `embedPath` gives for their target and
/// whether it is a wikilink, and are their words where it gives none, so Pandoc reads no other
/// file and fetches nothing from the web; a reference's local path outside the vault goes too.
/// A table of contents marker goes, and `toc` says whether there was one. Code is left alone.
[[nodiscard]] QString exportMarkdown(QStringView note,
                                     const std::function<QString(const QString &, bool)> &embedPath,
                                     bool *toc);

/// What to run Pandoc with to write `input`, a file of exportMarkdown(), to `output` as
/// `format`. `preset` is "standard" or "manuscript", standard manuscript format (Shunn), whose
/// files are in `presets`. `resources` is where relative image paths are found.
[[nodiscard]] QStringList pandocArguments(const QString &format, const QString &preset,
                                          const QString &input, const QString &output,
                                          const QString &title, bool toc, const QString &resources,
                                          const QString &presets);

} // namespace jot
