#include "jot/NoteScan.h"

#include "jot/TaskLine.h"
#include "jot/Vault.h"

#include <QLocale>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>
#include <functional>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// A checkbox line: its mark, then its words.
const QRegularExpression &checkboxLine()
{
    static const QRegularExpression line(uR"(^\s*(?:[-*+]|\d+[.)])\s+\[([ xX])\]\s*(.*?)\s*$)"_s);
    return line;
}

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

QList<Heading> allHeadingsOf(QStringView body)
{
    static const QRegularExpression heading(
        uR"(^ {0,3}((?:>\s?|[*+-]\s+|\d+[.)]\s+)*)(#{1,6})(?:\s+(.*?))?(?:\s+#+)?\s*$)"_s);
    // A line of = or - under a line of text makes it a heading, in a quote or a list item too:
    // the quote marks before each, and the list item's mark, set the two lines apart.
    static const QRegularExpression underline(uR"(^( *)((?:>\s?)*)( *)(=+|-+)\s*$)"_s);
    // A rule, or an underline, is no text to underline.
    static const QRegularExpression rule(uR"(^[\s>]*(?:[-*_=]\s*)+$)"_s);
    static const QRegularExpression text(
        uR"(^ {0,3}((?:>\s?)*) {0,3}((?:[*+-]|\d+[.)])\s+)?((?![#>|])\S.*)$)"_s);
    QList<Heading> found;
    int lastNumber = -1;
    qsizetype lastStart = 0;
    QString last;
    forEachProseLine(body, 1, [&](int number, qsizetype start, const QString &line) {
        if (const QRegularExpressionMatch m = heading.match(line); m.hasMatch()) {
            found.append({int(m.capturedLength(2)), m.captured(3).trimmed(), start,
                          m.capturedLength(1) > 0});
        } else if (const QRegularExpressionMatch u = underline.match(line);
                   u.hasMatch() && number == lastNumber + 1 && !rule.match(last).hasMatch()) {
            const QRegularExpressionMatch t = text.match(last);
            const auto quotes = [](const QString &marks) { return marks.count(u'>'); };
            const bool listed = t.hasMatch() && t.capturedLength(2) > 0;
            // Under a list item, only a line indented into the item underlines it; one at the
            // margin is a rule after the list.
            const qsizetype indent = u.capturedLength(1) + u.capturedLength(3);
            if (t.hasMatch() && quotes(t.captured(1)) == quotes(u.captured(2)) &&
                (listed ? indent >= 2 : indent <= 3))
                found.append({u.captured(4).front() == u'=' ? 1 : 2, t.captured(3).trimmed(),
                              lastStart, listed || quotes(u.captured(2)) > 0});
        }
        lastNumber = number;
        lastStart = start;
        last = line;
    });
    return found;
}

QList<Heading> headingsOf(QStringView body)
{
    QList<Heading> found = allHeadingsOf(body);
    found.removeIf([](const Heading &heading) { return heading.nested; });
    return found;
}

QList<std::pair<qsizetype, qsizetype>> inlineMath(QStringView line)
{
    static const QRegularExpression math(
        uR"((?<![\\$])\$(?![\s$])((?:\\.|[^$\\])+?)(?<![\s\\])\$(?![\d$]))"_s);
    QList<std::pair<qsizetype, qsizetype>> spans;
    if (!line.contains(u'$'))
        return spans;
    const QString prose = withoutCodeSpans(line.toString());
    for (const QRegularExpressionMatch &m : math.globalMatch(prose))
        spans.append({m.capturedStart(), m.capturedEnd()});
    return spans;
}

bool isMathFence(QStringView line, QString *math)
{
    const QStringView trimmed = line.trimmed();
    if (trimmed == u"$$")
        return true;
    if (trimmed.size() > 4 && trimmed.startsWith(u"$$") && trimmed.endsWith(u"$$")) {
        if (math)
            *math = trimmed.sliced(2, trimmed.size() - 4).trimmed().toString();
        return true;
    }
    return false;
}

bool isTocMarker(QStringView line)
{
    return line.trimmed() == u"${toc}";
}

namespace {

/// CriticMarkup read as Markdown: additions and highlights as their text, deletions struck
/// through, a substitution as both, and a comment in italics in brackets. Code is left alone.
QString criticForReading(QStringView body)
{
    static const QRegularExpression critic(
        uR"(\{\+\+(.+?)\+\+\}|\{--(.+?)--\}|\{~~(.*?)~>(.*?)~~\}|\{==(.+?)==\}|\{>>(.*?)<<\})"_s);
    if (!body.contains(u"{"_s))
        return body.toString();
    QString out;
    qsizetype copied = 0;
    forEachProseLine(body, 1, [&](int, qsizetype start, const QString &line) {
        const QString prose = withoutCodeSpans(line);
        qsizetype at = 0;
        bool changed = false;
        QString rewritten;
        for (const QRegularExpressionMatch &m : critic.globalMatch(prose)) {
            rewritten += QStringView(line).sliced(at, m.capturedStart() - at);
            const auto part = [&](int n) {
                return line.mid(m.capturedStart(n), m.capturedLength(n));
            };
            if (m.capturedLength(1) > 0)
                rewritten += part(1);
            else if (m.capturedLength(2) > 0)
                rewritten += u"~~"_s + part(2) + u"~~"_s;
            else if (m.capturedStart(3) >= 0)
                rewritten +=
                    (m.capturedLength(3) > 0 ? u"~~"_s + part(3) + u"~~ "_s : QString()) + part(4);
            else if (m.capturedLength(5) > 0)
                rewritten += part(5);
            else if (const QString comment = part(6).trimmed(); !comment.isEmpty())
                rewritten += u" *(%1)*"_s.arg(comment);
            at = m.capturedEnd();
            changed = true;
        }
        if (!changed)
            return;
        out += body.sliced(copied, start - copied);
        out += rewritten + line.mid(at);
        copied = start + line.size();
    });
    out += body.sliced(copied);
    return out;
}

/// Math as the pictures `mathUrl` gives for it: display math on a paragraph of its own, and
/// math within a line in its place. What it gives none for stays as written.
QString mathForReading(QStringView body,
                       const std::function<QString(const QString &, bool)> &mathUrl)
{
    static const QRegularExpression fence(uR"(^\s{0,3}(```+|~~~+))"_s);
    // The TeX is the picture's words, which Qt needs to place it at all, and a screen reader
    // reads.
    const auto picture = [&](const QString &tex, bool display) {
        const QString url = mathUrl(tex, display);
        QString words = tex.simplified();
        words.replace(u'\\', u"\\\\"_s).replace(u'[', u"\\["_s).replace(u']', u"\\]"_s);
        return url.isEmpty() ? QString() : u"![%1](<%2>)"_s.arg(words, url);
    };
    const QStringList lines = body.toString().split(u'\n');
    QStringList out;
    QString openFence;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        const QString &line = lines.at(i);
        if (const QRegularExpressionMatch m = fence.match(line);
            m.hasMatch() || !openFence.isEmpty()) {
            if (openFence.isEmpty())
                openFence = m.captured(1);
            else if (m.hasMatch() && m.captured(1).startsWith(openFence))
                openFence.clear();
            out << line;
            continue;
        }
        QString single;
        if (isMathFence(line, &single)) {
            qsizetype last = i;
            QString tex = single;
            if (single.isEmpty()) {
                last = i + 1;
                while (last < lines.size() && !isMathFence(lines.at(last)))
                    ++last;
                if (last == lines.size()) {
                    out << line;
                    continue;
                }
                tex = lines.mid(i + 1, last - i - 1).join(u'\n');
            }
            if (const QString drawn = picture(tex, true); !drawn.isEmpty()) {
                out << QString() << drawn << QString();
                i = last;
                continue;
            }
            out << line;
            continue;
        }
        QString rewritten;
        qsizetype at = 0;
        for (const auto &[start, end] : inlineMath(line)) {
            const QString drawn = picture(line.mid(start + 1, end - start - 2), false);
            if (drawn.isEmpty())
                continue;
            rewritten += line.mid(at, start - at) + drawn;
            at = end;
        }
        out << (at == 0 ? line : rewritten + line.mid(at));
    }
    return out.join(u'\n');
}

/// A day, "2026-11-02" or "2026-11-02T14:00", as people say it: "Nov 2", with its year when
/// that is not this one, and its time.
QString readableDay(const QString &value)
{
    const QLocale locale;
    const QDate day = QDate::fromString(value.left(10), Qt::ISODate);
    QString shown = locale.toString(
        day, day.year() == QDate::currentDate().year() ? u"MMM d"_s : u"MMM d, yyyy"_s);
    if (const QTime time = QTime::fromString(value.mid(11, 5), u"HH:mm"_s); time.isValid())
        shown += u", "_s + locale.toString(time, QLocale::ShortFormat);
    return shown;
}

} // namespace

QString readingMarkdown(QStringView original,
                        const std::function<QString(const QString &)> &embedUrl,
                        const std::function<QString(const QString &, bool)> &mathUrl)
{
    const QString accepted =
        criticForReading(mathUrl ? mathForReading(original, mathUrl) : original.toString());
    const QStringView body = accepted;
    // A wikilink with its label, after a bar, when it has one.
    static const QRegularExpression wiki(uR"((!?)\[\[([^\[\]|]*?)(?:\|([^\[\]]*))?\]\])"_s);
    const auto encode = [](const QString &target) {
        return QString::fromLatin1(QUrl::toPercentEncoding(target.trimmed(), "/#"));
    };
    const auto escape = [](QString label) {
        return label.replace(u'[', u"\\["_s).replace(u']', u"\\]"_s);
    };
    const QList<Heading> headings = headingsOf(body);
    QString contents;
    if (!headings.isEmpty()) {
        const int top = std::ranges::min(headings, {}, &Heading::level).level;
        contents = u"\n"_s;
        for (qsizetype i = 0; i < headings.size(); ++i)
            contents += u"%1- [%2](jot-heading:%3)\n"_s.arg(
                QString(2 * (headings.at(i).level - top), u' '), escape(headings.at(i).text),
                QString::number(i));
    }
    QString out;
    qsizetype copied = 0;
    forEachProseLine(body, 1, [&](int, qsizetype start, const QString &line) {
        if (isTocMarker(line)) {
            out += body.sliced(copied, start - copied);
            out += contents;
            copied = start + line.size();
            return;
        }
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
            if (!m.captured(1).isEmpty() && Vault::isAudioFile(target)) {
                text = u"[%1](jot-wiki-sound:%2)"_s.arg(escape(label), encode(target));
            } else if (!m.captured(1).isEmpty()) {
                if (const QString url = embedUrl(target); !url.isEmpty())
                    text = u"![%1](<%2>)"_s.arg(escape(label), url);
            }
            changes.append({m.capturedStart(), m.capturedLength(), text});
            rest.replace(m.capturedStart(), m.capturedLength(), QString(m.capturedLength(), u' '));
        }
        // Tags, but not the # in a link's target or label, such as a heading's anchor.
        for (const QRegularExpressionMatch &m : markdownPattern().globalMatch(rest)) {
            const QString target = m.captured(2);
            const QString bare =
                target.startsWith(u'<') ? target.mid(1, target.size() - 2) : target;
            // A sound is a link the reader can play, named for its file when it has no words.
            if (!m.captured(1).isEmpty() && Vault::isAudioFile(bare)) {
                static const QRegularExpression words(uR"(^!\[((?:[^\[\]]|\[[^\[\]]*\])*)\])"_s);
                QString label = words.match(m.captured()).captured(1);
                if (label.isEmpty())
                    label = escape(QUrl::fromPercentEncoding(bare.section(u'/', -1).toUtf8()));
                changes.append({m.capturedStart(), m.capturedLength(),
                                u"[%1](jot-sound:%2)"_s.arg(label, encode(bare))});
            } else if (!m.captured(1).isEmpty() &&
                       isUrl(target.startsWith(u'<') ? target.mid(1) : target) &&
                       !target.contains(u"file:"_s)) {
                // An image from the web is a link to it, to open elsewhere.
                changes.append({m.capturedStart(), 1, {}});
            } else if (!m.captured(1).isEmpty() && m.captured().startsWith(u"![]("_s)) {
                // Qt draws an image only once it reads its words, so one without any is named
                // for its file.
                changes.append({m.capturedStart(), 3,
                                u"![%1]"_s.arg(escape(QUrl::fromPercentEncoding(
                                    bare.section(u'#', 0, 0).section(u'/', -1).toUtf8())))});
            }
            rest.replace(m.capturedStart(), m.capturedLength(), QString(m.capturedLength(), u' '));
        }
        // A to-do's markers, said as a reader would: 🔁 weekly, 📅 Nov 2, ✅ Nov 1, 🚩 Urgent.
        if (const QRegularExpressionMatch box = checkboxLine().match(prose); box.hasMatch()) {
            const qsizetype from = box.capturedStart(2);
            for (const TaskMarker &marker : taskMarkers(QStringView(rest).sliced(from))) {
                const qsizetype at = from + marker.start;
                const qsizetype length = marker.end - marker.start;
                QString shown;
                if (marker.kind == TaskMarker::Repeat)
                    shown = u"🔁 "_s + line.mid(at + 1, length - 1);
                else if (marker.kind == TaskMarker::Priority)
                    shown = u"🚩 "_s + priorityName(marker.value.toInt());
                else
                    shown = (marker.kind == TaskMarker::Due ? u"📅 "_s : u"✅ "_s) +
                            readableDay(marker.value);
                changes.append({at, length, shown});
                rest.replace(at, length, QString(length, u' '));
            }
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

std::optional<QString> setTaskDone(const QString &note, int line, bool done, const QString &text,
                                   const QDate &today)
{
    const QRegularExpression &task = checkboxLine();
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
    if (!done) {
        changed[mark] = u' ';
        // Open again, it was not done on the day @done says.
        qsizetype end = changed.indexOf(u'\n', mark);
        if (end < 0)
            end = changed.size();
        const QString line = changed.sliced(mark, end - mark);
        return changed.replace(mark, end - mark, withoutMarkers(line, {TaskMarker::Done}));
    }
    const TickEdit edit = tickedLineAt(note, mark, today);
    return changed.replace(edit.start, edit.end - edit.start, edit.text);
}

std::optional<TaskWords> taskWordsAt(QStringView note, qsizetype position)
{
    position = std::clamp<qsizetype>(position, 0, note.size());
    const qsizetype start = position > 0 ? note.lastIndexOf(u'\n', position - 1) + 1 : 0;
    qsizetype end = note.indexOf(u'\n', position);
    if (end < 0)
        end = note.size();
    const QRegularExpressionMatch m = checkboxLine().matchView(note.sliced(start, end - start));
    if (!m.hasMatch())
        return std::nullopt;
    return TaskWords{start + m.capturedStart(2), start + m.capturedEnd(2)};
}

TickEdit tickedLineAt(QStringView note, qsizetype position, const QDate &today)
{
    position = std::clamp<qsizetype>(position, 0, note.size());
    const qsizetype start = position > 0 ? note.lastIndexOf(u'\n', position - 1) + 1 : 0;
    qsizetype end = note.indexOf(u'\n', position);
    if (end < 0)
        end = note.size();
    return {start, end, completedTaskLine(note.sliced(start, end - start).toString(), today)};
}

int bodyFirstLine(QStringView note, QStringView body)
{
    const qsizetype offset = body.data() - note.data();
    if (offset <= 0 || offset > note.size())
        return 1;
    return int(note.first(offset).count(u'\n')) + 1;
}

} // namespace jot
