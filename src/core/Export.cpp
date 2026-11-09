#include "jot/Export.h"

#include "jot/FrontMatter.h"
#include "jot/NoteScan.h"

#include <QDir>
#include <QObject>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace jot {

QList<ExportFormat> exportFormats()
{
    return {{u"docx"_s, QObject::tr("Word document (.docx)"), u"docx"_s},
            {u"odt"_s, QObject::tr("OpenDocument text (.odt)"), u"odt"_s},
            {u"pdf"_s, QObject::tr("PDF"), u"pdf"_s},
            {u"epub"_s, QObject::tr("ePub book"), u"epub"_s},
            {u"html"_s, QObject::tr("Web page (.html)"), u"html"_s}};
}

QString exportMarkdown(QStringView note, const std::function<QString(const QString &)> &embedPath,
                       bool *toc)
{
    static const QRegularExpression critic(
        uR"(\{\+\+(.+?)\+\+\}|\{--(.+?)--\}|\{~~(.*?)~>(.*?)~~\}|\{==(.+?)==\}|\{>>(.*?)<<\})"_s);
    static const QRegularExpression wiki(uR"((!?)\[\[([^\[\]|]*?)(?:\|([^\[\]]*))?\]\])"_s);
    const QString body = splitFrontMatter(note).body.toString();
    *toc = false;
    QStringList lines;
    // Code is left as written: a fence closes only with its own mark, at least as long, and a
    // line four spaces or a tab in is code unless it is part of a list.
    QString openFence;
    static const QRegularExpression fence(uR"(^\s{0,3}(```+|~~~+))"_s);
    static const QRegularExpression indented(uR"(^(?: {4}|\t)(?!\s*(?:[-*+]|\d+[.)])\s))"_s);
    for (const QString &line : body.split(u'\n')) {
        const QRegularExpressionMatch mark = fence.match(line);
        if (!openFence.isEmpty()) {
            if (mark.hasMatch() && mark.captured(1).front() == openFence.front() &&
                mark.captured(1).size() >= openFence.size())
                openFence.clear();
            lines << line;
            continue;
        }
        if (mark.hasMatch()) {
            openFence = mark.captured(1);
            lines << line;
            continue;
        }
        if (indented.match(line).hasMatch()) {
            lines << line;
            continue;
        }
        if (isTocMarker(line)) {
            *toc = true;
            continue;
        }
        // Code spans keep what is in them, so matches are found with them blanked.
        QString text = line;
        for (const QRegularExpression *pattern : {&critic, &wiki}) {
            static const QRegularExpression span(uR"((`+)(.+?)\1)"_s);
            QString prose = text;
            for (const QRegularExpressionMatch &m : span.globalMatch(text))
                prose.replace(m.capturedStart(), m.capturedLength(),
                              QString(m.capturedLength(), u' '));
            QString out;
            qsizetype at = 0;
            for (const QRegularExpressionMatch &m : pattern->globalMatch(prose)) {
                const auto part = [&](int n) {
                    return text.mid(m.capturedStart(n), m.capturedLength(n));
                };
                out += QStringView(text).sliced(at, m.capturedStart() - at);
                if (pattern == &critic) {
                    // Edits made: what was added or put in, never what went or a comment.
                    out += part(1) + part(4) + part(5);
                } else {
                    const QString target = part(2).trimmed();
                    const QString label = part(3).isEmpty() ? target : part(3).trimmed();
                    const QString path = part(1).isEmpty() ? QString() : embedPath(target);
                    out += path.isEmpty() ? label : u"![%1](<%2>)"_s.arg(label, path);
                }
                at = m.capturedEnd();
            }
            text = out + text.mid(at);
        }
        lines << text;
    }
    return lines.join(u'\n');
}

QStringList pandocArguments(const QString &format, const QString &preset, const QString &input,
                            const QString &output, const QString &title, bool toc,
                            const QString &resources, const QString &presets)
{
    QStringList arguments{u"--from=markdown"_s,
                          u"--to=%1"_s.arg(format == u"pdf"_s ? u"pdf"_s : format),
                          u"--output=%1"_s.arg(output), u"--resource-path=%1"_s.arg(resources),
                          u"--metadata=title:%1"_s.arg(title)};
    if (format == u"html"_s || format == u"pdf"_s)
        arguments << u"--standalone"_s;
    if (format == u"html"_s)
        arguments << u"--embed-resources"_s;
    if (toc)
        arguments << u"--toc"_s;
    if (preset == u"manuscript"_s) {
        const QDir files(presets);
        if (format == u"docx"_s || format == u"odt"_s)
            arguments << u"--reference-doc=%1"_s.arg(files.filePath(u"manuscript."_s + format));
        else if (format == u"pdf"_s)
            arguments << u"--variable=fontfamily:courier"_s << u"--variable=fontsize:12pt"_s
                      << u"--variable=linestretch:2"_s << u"--variable=geometry:margin=1in"_s;
        else
            arguments << u"--css=%1"_s.arg(files.filePath(u"manuscript.css"_s));
    }
    arguments << input;
    return arguments;
}

} // namespace jot
