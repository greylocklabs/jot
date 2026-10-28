#include "jot/TaskLine.h"

#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace jot {

TaskLine readTaskLine(const QString &text)
{
    // Obsidian Tasks' markers: due, scheduled, start, created, done, cancelled, repeats, and
    // the priorities. Each is followed by its value, up to the next marker.
    static const QRegularExpression marker(u"(📅|⏳|🛫|➕|✅|❌|🔁|⏫|🔼|🔽|🔺|⏬)\\x{FE0F}?"_s);
    static const QRegularExpression date(uR"(^\s*(\d{4}-\d{2}-\d{2})(?:[ T](\d{1,2}:\d{2}))?)"_s);
    static const QRegularExpression label(uR"((?:^|(?<=\s))@([\p{L}\p{N}_-]+))"_s);

    TaskLine line;
    // Everything before the first marker is the to-do's own words; each marker owns what
    // follows it up to the next.
    QList<QRegularExpressionMatch> found;
    for (const QRegularExpressionMatch &m : marker.globalMatch(text))
        found.append(m);
    QString words = found.isEmpty() ? text : text.first(found.first().capturedStart());
    for (qsizetype i = 0; i < found.size(); ++i) {
        const QRegularExpressionMatch &m = found.at(i);
        const qsizetype end = i + 1 < found.size() ? found.at(i + 1).capturedStart() : text.size();
        const QString value = text.sliced(m.capturedEnd(), end - m.capturedEnd()).trimmed();
        const QString kind = m.captured(1);
        const QRegularExpressionMatch day = date.match(value);
        if (kind == u"📅"_s && day.hasMatch()) {
            line.due =
                day.captured(2).isEmpty()
                    ? day.captured(1)
                    : u"%1T%2"_s.arg(day.captured(1), day.captured(2).rightJustified(5, u'0'));
        } else if (kind == u"✅"_s && day.hasMatch()) {
            line.done = day.captured(1);
        } else if (kind == u"🔁"_s) {
            line.repeat = value;
        }
        // Words after a value, such as a tag written after the date, still belong to the to-do.
        if (kind != u"🔁"_s) {
            const QString rest =
                day.hasMatch() ? value.sliced(day.capturedLength()).trimmed() : QString();
            if (!rest.isEmpty())
                words += u' ' + rest;
        }
    }
    for (const QRegularExpressionMatch &m : label.globalMatch(words)) {
        if (!line.labels.contains(m.captured(1), Qt::CaseInsensitive))
            line.labels.append(m.captured(1));
    }
    words.remove(label);
    line.title = words.simplified();
    return line;
}

} // namespace jot
