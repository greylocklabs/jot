#include "jot/NoteScan.h"

#include <QRegularExpression>
#include <QUrl>

#include <algorithm>
#include <functional>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// `line` with its code spans blanked out, so their contents match nothing.
QString withoutCodeSpans(const QString &line)
{
    QString result = line;
    qsizetype i = 0;
    while (i < result.size()) {
        if (result[i] != u'`') {
            ++i;
            continue;
        }
        qsizetype ticks = 0;
        while (i + ticks < result.size() && result[i + ticks] == u'`')
            ++ticks;
        const QString fence(ticks, u'`');
        const qsizetype close = result.indexOf(fence, i + ticks);
        if (close < 0)
            break;
        for (qsizetype j = i; j < close + ticks; ++j)
            result[j] = u' ';
        i = close + ticks;
    }
    return result;
}

bool isUrl(const QString &target)
{
    static const QRegularExpression scheme(u"^[A-Za-z][A-Za-z0-9+.-]*:"_s);
    return scheme.match(target).hasMatch();
}

const QRegularExpression &wikiPattern()
{
    static const QRegularExpression wiki(uR"((!?)\[\[([^\[\]|]*?)(?:\|[^\[\]]*)?\]\])"_s);
    return wiki;
}

const QRegularExpression &markdownPattern()
{
    static const QRegularExpression markdown(
        uR"((!?)\[(?:[^\[\]]|\[[^\[\]]*\])*\]\(\s*(<[^>]*>|[^\s)]+)(?:\s+(?:"[^"]*"|'[^']*'))?\s*\))"_s);
    return markdown;
}

/// Calls `visit` with each line of `body` that is prose rather than code: its number in the
/// file, where it starts in `body`, and its text with code spans blanked out.
void forEachProseLine(QStringView body, int firstLine,
                      const std::function<void(int, qsizetype, const QString &)> &visit)
{
    static const QRegularExpression fence(uR"(^\s{0,3}(```+|~~~+))"_s);
    QString openFence;
    int number = firstLine - 1;
    qsizetype offset = 0;
    while (offset <= body.size()) {
        qsizetype end = body.indexOf(u'\n', offset);
        if (end < 0)
            end = body.size();
        ++number;
        QString line = body.sliced(offset, end - offset).toString();
        const qsizetype start = offset;
        offset = end + 1;
        if (line.endsWith(u'\r'))
            line.chop(1);

        const QRegularExpressionMatch fenceMatch = fence.match(line);
        if (!openFence.isEmpty()) {
            if (fenceMatch.hasMatch() && fenceMatch.captured(1).front() == openFence.front() &&
                fenceMatch.captured(1).size() >= openFence.size())
                openFence.clear();
            continue;
        }
        if (fenceMatch.hasMatch()) {
            openFence = fenceMatch.captured(1);
            continue;
        }
        // Indented code, four spaces or a tab in, unless it is part of a list.
        if ((line.startsWith(u"    "_s) || line.startsWith(u'\t')) &&
            !QStringView(line).trimmed().startsWith(u'-') &&
            !QStringView(line).trimmed().startsWith(u'*'))
            continue;
        visit(number, start, line);
    }
}

const QRegularExpression &tagPattern()
{
    static const QRegularExpression tag(
        uR"((?:^|(?<=[\s(,]))#([\p{L}\p{N}_/-]*[\p{L}_/-][\p{L}\p{N}_/-]*))"_s);
    return tag;
}

} // namespace

NoteScan scanNote(QStringView body, int firstLine)
{
    const QRegularExpression &tag = tagPattern();
    static const QRegularExpression task(uR"(^\s*(?:[-*+]|\d+[.)])\s+\[([ xX])\]\s+(.*)$)"_s);

    NoteScan scan;
    forEachProseLine(body, firstLine, [&](int number, qsizetype, const QString &line) {
        if (const QRegularExpressionMatch m = task.match(line); m.hasMatch())
            scan.tasks.append({number, m.captured(1) != u" ", m.captured(2).trimmed()});

        const QString text = withoutCodeSpans(line);
        QString rest = text;
        // Each with where it starts, so the line's links keep the order they are written in.
        QList<std::pair<qsizetype, NoteScan::Link>> found;
        for (const QRegularExpressionMatch &m : wikiPattern().globalMatch(text)) {
            found.append({m.capturedStart(),
                          {m.captured(2).trimmed(),
                           m.captured(1).isEmpty() ? u"wiki"_s : u"embed"_s, number}});
            // Blanked so the tag search does not read "[[#Heading]]" as a tag.
            rest.replace(m.capturedStart(), m.capturedLength(), QString(m.capturedLength(), u' '));
        }
        for (const QRegularExpressionMatch &m : markdownPattern().globalMatch(rest)) {
            QString target = m.captured(2);
            if (target.startsWith(u'<'))
                target = target.mid(1, target.size() - 2);
            if (!target.isEmpty() && !target.startsWith(u'#') && !isUrl(target))
                found.append({m.capturedStart(),
                              {target, m.captured(1).isEmpty() ? u"link"_s : u"embed"_s, number}});
            rest.replace(m.capturedStart(), m.capturedLength(), QString(m.capturedLength(), u' '));
        }
        std::ranges::sort(found, {}, &std::pair<qsizetype, NoteScan::Link>::first);
        for (const auto &[at, link] : std::as_const(found))
            scan.links.append(link);
        for (const QRegularExpressionMatch &m : tag.globalMatch(rest)) {
            QString name = m.captured(1);
            while (name.endsWith(u'/'))
                name.chop(1);
            if (!name.isEmpty() && !scan.tags.contains(name, Qt::CaseInsensitive))
                scan.tags.append(name);
        }
    });
    return scan;
}

QString readingMarkdown(QStringView body, const std::function<QString(const QString &)> &embedUrl)
{
    // A wikilink with its label, after a bar, when it has one.
    static const QRegularExpression wiki(uR"((!?)\[\[([^\[\]|]*?)(?:\|([^\[\]]*))?\]\])"_s);
    const auto encode = [](const QString &target) {
        return QString::fromLatin1(QUrl::toPercentEncoding(target.trimmed(), "/#"));
    };
    const auto escape = [](QString label) {
        return label.replace(u'[', u"\\["_s).replace(u']', u"\\]"_s);
    };
    QString out;
    qsizetype copied = 0;
    forEachProseLine(body, 1, [&](int, qsizetype start, const QString &line) {
        // Each replacement by where it starts in the line, and how long a stretch it replaces.
        struct Change
        {
            qsizetype at;
            qsizetype length;
            QString text;
        };
        QList<Change> changes;
        // Matched with code spans blanked, so what is in them stays as written.
        const QString prose = withoutCodeSpans(line);
        QString rest = prose;
        for (const QRegularExpressionMatch &m : wiki.globalMatch(prose)) {
            const QString target = m.captured(2).trimmed();
            const QString label = m.captured(3).isEmpty() ? target : m.captured(3).trimmed();
            QString text = u"[%1](jot-note:%2)"_s.arg(escape(label), encode(target));
            if (!m.captured(1).isEmpty()) {
                if (const QString url = embedUrl(target); !url.isEmpty())
                    text = u"![%1](<%2>)"_s.arg(escape(label), url);
            }
            changes.append({m.capturedStart(), m.capturedLength(), text});
            rest.replace(m.capturedStart(), m.capturedLength(), QString(m.capturedLength(), u' '));
        }
        // Tags, but not the # in a link's target or label, such as a heading's anchor.
        for (const QRegularExpressionMatch &m : markdownPattern().globalMatch(rest)) {
            // An image from the web becomes a link to it, so reading fetches nothing.
            const QString target = m.captured(2);
            if (!m.captured(1).isEmpty() &&
                isUrl(target.startsWith(u'<') ? target.mid(1) : target) &&
                !target.contains(u"file:"_s))
                changes.append({m.capturedStart(), 1, {}});
            rest.replace(m.capturedStart(), m.capturedLength(), QString(m.capturedLength(), u' '));
        }
        for (const QRegularExpressionMatch &m : tagPattern().globalMatch(rest)) {
            QString name = m.captured(1);
            while (name.endsWith(u'/'))
                name.chop(1);
            if (name.isEmpty())
                continue;
            changes.append({m.capturedStart(), name.size() + 1,
                            u"[#%1](jot-tag:%2)"_s.arg(escape(name), encode(name))});
        }
        if (changes.isEmpty())
            return;
        std::ranges::sort(changes, {}, &Change::at);
        out += body.sliced(copied, start - copied);
        qsizetype at = 0;
        for (const Change &change : std::as_const(changes)) {
            out += QStringView(line).sliced(at, change.at - at);
            out += change.text;
            at = change.at + change.length;
        }
        out += QStringView(line).sliced(at);
        copied = start + line.size();
    });
    out += body.sliced(copied);
    return out;
}

QString
rewriteLinks(QStringView body,
             const std::function<std::optional<QString>(const QString &raw, bool wiki)> &replace)
{
    struct Edit
    {
        qsizetype start;
        qsizetype length;
        QString text;
    };
    QList<Edit> edits;
    forEachProseLine(body, 1, [&](int, qsizetype lineStart, const QString &line) {
        const QString text = withoutCodeSpans(line);
        QString rest = text;
        for (const QRegularExpressionMatch &m : wikiPattern().globalMatch(text)) {
            const QString raw = m.captured(2).trimmed();
            if (const std::optional<QString> with = replace(raw, true))
                edits.append({lineStart + m.capturedStart(2), m.capturedLength(2), *with});
            rest.replace(m.capturedStart(), m.capturedLength(), QString(m.capturedLength(), u' '));
        }
        for (const QRegularExpressionMatch &m : markdownPattern().globalMatch(rest)) {
            QString target = m.captured(2);
            const bool bracketed = target.startsWith(u'<');
            if (bracketed)
                target = target.mid(1, target.size() - 2);
            if (target.isEmpty() || target.startsWith(u'#') || isUrl(target))
                continue;
            if (std::optional<QString> with = replace(target, false)) {
                // Spaces need the angle brackets, or %20, to stay one target; a parenthesis
                // would end it, so it takes the brackets.
                if (bracketed || with->contains(u')') || with->contains(u'('))
                    *with = u'<' + *with + u'>';
                else
                    with->replace(u' ', u"%20"_s);
                edits.append({lineStart + m.capturedStart(2), m.capturedLength(2), *with});
            }
        }
    });
    QString result = body.toString();
    std::ranges::sort(edits, std::greater{}, &Edit::start);
    for (const Edit &edit : std::as_const(edits))
        result.replace(edit.start, edit.length, edit.text);
    return result;
}

qsizetype lineStart(QStringView text, int line)
{
    qsizetype start = 0;
    for (int number = 1; number < line; ++number) {
        const qsizetype next = text.indexOf(u'\n', start);
        if (next < 0)
            return text.size();
        start = next + 1;
    }
    return start;
}

std::optional<QString> setTaskDone(const QString &note, int line, bool done, const QString &text)
{
    static const QRegularExpression task(uR"(^\s*(?:[-*+]|\d+[.)])\s+\[([ xX])\]\s*(.*?)\s*$)"_s);
    // The mark's place in the note, if `number` is a task that says `text` (when given).
    const auto markOn = [&](int number) -> qsizetype {
        const qsizetype start = lineStart(note, number);
        if (start >= note.size() && number > 1)
            return -1;
        qsizetype end = note.indexOf(u'\n', start);
        if (end < 0)
            end = note.size();
        const QRegularExpressionMatch m =
            task.matchView(QStringView(note).sliced(start, end - start));
        if (!m.hasMatch() || (!text.isEmpty() && m.captured(2) != text.trimmed()))
            return -1;
        return start + m.capturedStart(1);
    };
    qsizetype mark = markOn(line);
    if (mark < 0 && !text.isEmpty()) {
        // Lines moved: the task is the one, and only one, that says the same.
        const int lines = int(note.count(u'\n')) + 1;
        for (int number = 1; number <= lines; ++number) {
            const qsizetype found = markOn(number);
            if (found < 0)
                continue;
            if (mark >= 0)
                return std::nullopt;
            mark = found;
        }
    }
    if (mark < 0)
        return std::nullopt;
    QString changed = note;
    changed[mark] = done ? u'x' : u' ';
    return changed;
}

int bodyFirstLine(QStringView note, QStringView body)
{
    const qsizetype offset = body.data() - note.data();
    if (offset <= 0 || offset > note.size())
        return 1;
    return int(note.first(offset).count(u'\n')) + 1;
}

} // namespace jot
