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
            // A label or a tag after the rule is the to-do's, not part of the rule.
            QStringList rule;
            for (const QString &word : value.split(u' ', Qt::SkipEmptyParts)) {
                if (word.startsWith(u'@') || word.startsWith(u'#'))
                    words += u' ' + word;
                else
                    rule << word;
            }
            line.repeat = rule.join(u' ');
            continue;
        }
        // Words after a date, such as a tag, and a value that is no date at all, still belong
        // to the to-do.
        const QString rest = day.hasMatch() ? value.sliced(day.capturedLength()).trimmed() : value;
        if (!rest.isEmpty())
            words += u' ' + rest;
    }
    for (const QRegularExpressionMatch &m : label.globalMatch(words)) {
        if (!line.labels.contains(m.captured(1), Qt::CaseInsensitive))
            line.labels.append(m.captured(1));
    }
    words.remove(label);
    line.title = words.simplified();
    return line;
}

QDate nextOccurrence(const QString &rule, const QDate &from)
{
    static const QRegularExpression every(
        uR"(^every\s+(?:(other|\d+)\s+)?(day|week|month|year|weekday|monday|tuesday|wednesday|thursday|friday|saturday|sunday)s?$)"_s,
        QRegularExpression::CaseInsensitiveOption);
    static const QStringList weekdays = {u"monday"_s, u"tuesday"_s,  u"wednesday"_s, u"thursday"_s,
                                         u"friday"_s, u"saturday"_s, u"sunday"_s};
    const QRegularExpressionMatch m = every.match(rule.simplified());
    if (!m.hasMatch() || !from.isValid())
        return {};
    const QString count = m.captured(1).toLower();
    const int n = count.isEmpty() ? 1 : count == u"other" ? 2 : count.toInt();
    const QString step = m.captured(2).toLower();
    if (n < 1)
        return {};
    if (step == u"day")
        return from.addDays(n);
    if (step == u"week")
        return from.addDays(7 * n);
    if (step == u"month")
        return from.addMonths(n);
    if (step == u"year")
        return from.addYears(n);
    if (step == u"weekday") {
        QDate next = from.addDays(1);
        while (next.dayOfWeek() > 5)
            next = next.addDays(1);
        return next;
    }
    const int wanted = int(weekdays.indexOf(step)) + 1;
    return from.addDays((wanted - from.dayOfWeek() + 6) % 7 + 1);
}

QString completedTaskLine(const QString &line, const QDate &today)
{
    static const QRegularExpression box(
        uR"(^(\s*(?:[-*+]|\d+[.)])\s+\[)[ xX](\]\s*)(.*?)(\r?)$)"_s);
    static const QRegularExpression due(u"📅\\x{FE0F}?\\s*(\\d{4}-\\d{2}-\\d{2})"_s);
    const QRegularExpressionMatch m = box.match(line);
    if (!m.hasMatch())
        return line;
    const QString text = m.captured(3);
    const QString done = m.captured(1) + u'x' + m.captured(2) + text;
    const TaskLine parts = readTaskLine(text);
    const QDate was = QDate::fromString(parts.due.left(10), Qt::ISODate);
    QDate next = nextOccurrence(parts.repeat, was);
    if (!next.isValid())
        return done + m.captured(4);
    // Done late, it comes round again after today rather than on a day already gone.
    while (next <= today)
        next = nextOccurrence(parts.repeat, next);
    QString again = text;
    const QRegularExpressionMatch day = due.match(again);
    again.replace(day.capturedStart(1), day.capturedLength(1), next.toString(Qt::ISODate));
    // The next one carries no done date of its own.
    static const QRegularExpression doneMark(u"\\s*✅\\x{FE0F}?\\s*\\d{4}-\\d{2}-\\d{2}"_s);
    again.remove(doneMark);
    QString completed = done;
    completed.remove(doneMark);
    const QString ending = m.captured(4);
    return m.captured(1) + u' ' + m.captured(2) + again + ending + u'\n' + completed + u" ✅ "_s +
           today.toString(Qt::ISODate) + ending;
}

} // namespace jot
