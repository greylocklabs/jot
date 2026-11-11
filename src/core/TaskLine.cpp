#include "jot/TaskLine.h"

#include <QHash>
#include <QObject>
#include <QRegularExpression>
#include <QStringList>

using namespace Qt::StringLiterals;

namespace jot {

QList<TaskMarker> taskMarkers(QStringView text)
{
    static const QRegularExpression word(uR"((?:^|(?<=\s))@([A-Za-z]+[1-4]?)(?=\s|$))"_s);
    static const QRegularExpression day(
        uR"(^\s+(\d{4}-\d{2}-\d{2})(?:[ T](\d{1,2}:\d{2}))?(?=\s|$))"_s);
    static const QHash<QString, QString> every = {{u"daily"_s, u"every day"_s},
                                                  {u"weekly"_s, u"every week"_s},
                                                  {u"monthly"_s, u"every month"_s},
                                                  {u"yearly"_s, u"every year"_s},
                                                  {u"weekdays"_s, u"every weekday"_s}};
    QList<TaskMarker> found;
    for (const QRegularExpressionMatch &m : word.globalMatchView(text)) {
        const QString name = m.captured(1).toLower();
        const qsizetype start = m.capturedStart();
        if (const int priority = priorityOf(name); priority > 0) {
            found.append({TaskMarker::Priority, start, m.capturedEnd(), QString::number(priority)});
        } else if (const auto shortcut = every.constFind(name); shortcut != every.cend()) {
            found.append({TaskMarker::Repeat, start, m.capturedEnd(), *shortcut});
        } else if (name == u"every") {
            // The rule's words, up to the first that no rule is made of, so words after it stay
            // the to-do's own.
            static const QRegularExpression ruleWord(
                uR"(^(?:\d+|other|on|(?:day|week|month|year|weekday|monday|tuesday|wednesday|thursday|friday|saturday|sunday)s?)$)"_s,
                QRegularExpression::CaseInsensitiveOption);
            QStringList rule;
            qsizetype at = m.capturedEnd();
            qsizetype end = at;
            while (true) {
                while (at < text.size() && text.at(at).isSpace())
                    ++at;
                const qsizetype from = at;
                while (at < text.size() && !text.at(at).isSpace())
                    ++at;
                const QString word = text.sliced(from, at - from).toString();
                if (word.isEmpty() || !ruleWord.match(word).hasMatch())
                    break;
                rule.append(word);
                end = at;
            }
            if (!rule.isEmpty())
                found.append({TaskMarker::Repeat, start, end, u"every "_s + rule.join(u' ')});
        } else if (name == u"due" || name == u"done") {
            const QRegularExpressionMatch date = day.matchView(text.sliced(m.capturedEnd()));
            if (!date.hasMatch() || !QDate::fromString(date.captured(1), Qt::ISODate).isValid())
                continue;
            // A time goes with a due day; a done one is the day alone.
            const bool due = name == u"due";
            const QString time = due ? date.captured(2) : QString();
            const qsizetype end =
                m.capturedEnd() + (due ? date.capturedEnd() : date.capturedEnd(1));
            found.append({due ? TaskMarker::Due : TaskMarker::Done, start, end,
                          time.isEmpty()
                              ? date.captured(1)
                              : u"%1T%2"_s.arg(date.captured(1), time.rightJustified(5, u'0'))});
        }
    }
    return found;
}

TaskLine readTaskLine(const QString &text)
{
    TaskLine line;
    for (const TaskMarker &marker : taskMarkers(text)) {
        if (marker.kind == TaskMarker::Priority) {
            if (line.priority == 0)
                line.priority = marker.value.toInt();
            continue;
        }
        QString &value = marker.kind == TaskMarker::Due      ? line.due
                         : marker.kind == TaskMarker::Repeat ? line.repeat
                                                             : line.done;
        if (value.isEmpty())
            value = marker.value;
    }
    line.title = withoutMarkers(text, {TaskMarker::Due, TaskMarker::Repeat, TaskMarker::Done,
                                       TaskMarker::Priority})
                     .simplified();
    return line;
}

QList<MarkerSuggestion> markerSuggestions(const QString &typed, const QDate &today)
{
    // Next Monday, as "next week" means in quick add.
    const QDate monday = today.addDays(8 - today.dayOfWeek());
    const auto due = [](const QDate &day) { return dueMarker(day.toString(Qt::ISODate)); };
    const QList<MarkerSuggestion> all = {
        {due(today), u"@due today"_s, QObject::tr("Due today")},
        {due(today.addDays(1)), u"@due tomorrow"_s, QObject::tr("Due tomorrow")},
        {due(monday), u"@due next week"_s, QObject::tr("Due next Monday")},
        {u"@due "_s, u"@due …"_s, QObject::tr("Due on a day: 2026-11-02, with 14:00 after it")},
        {u"@daily"_s, u"@daily"_s, QObject::tr("Repeats every day")},
        {u"@weekly"_s, u"@weekly"_s, QObject::tr("Repeats every week")},
        {u"@weekdays"_s, u"@weekdays"_s, QObject::tr("Repeats Monday to Friday")},
        {u"@monthly"_s, u"@monthly"_s, QObject::tr("Repeats every month")},
        {u"@yearly"_s, u"@yearly"_s, QObject::tr("Repeats every year")},
        {u"@every "_s, u"@every …"_s, QObject::tr("Repeats as written: every 2 weeks on monday")},
        {doneMarker(today), u"@done today"_s, QObject::tr("Done today")},
        {u"@p1"_s, u"@p1"_s, QObject::tr("Urgent priority")},
        {u"@p2"_s, u"@p2"_s, QObject::tr("High priority")},
        {u"@p3"_s, u"@p3"_s, QObject::tr("Medium priority")},
        {u"@p4"_s, u"@p4"_s, QObject::tr("Low priority")},
    };
    QList<MarkerSuggestion> found;
    const QString wanted = typed.toLower();
    for (const MarkerSuggestion &each : all) {
        if (each.shown.mid(1).startsWith(wanted))
            found.append(each);
    }
    return found;
}

QString repeatMarker(const QString &rule)
{
    static const QHash<QString, QString> shortcuts = {{u"every day"_s, u"@daily"_s},
                                                      {u"every week"_s, u"@weekly"_s},
                                                      {u"every month"_s, u"@monthly"_s},
                                                      {u"every year"_s, u"@yearly"_s},
                                                      {u"every weekday"_s, u"@weekdays"_s}};
    const QString tidy = rule.simplified();
    if (const auto shortcut = shortcuts.constFind(tidy.toLower()); shortcut != shortcuts.cend())
        return *shortcut;
    return tidy.startsWith(u"every "_s, Qt::CaseInsensitive) ? u'@' + tidy : u"@every "_s + tidy;
}

QString dueMarker(const QString &due)
{
    return u"@due "_s + QString(due).replace(u'T', u' ');
}

QString doneMarker(const QDate &day)
{
    return u"@done "_s + day.toString(Qt::ISODate);
}

QString priorityMarker(int priority)
{
    return priority >= 1 && priority <= 4 ? u"@p"_s + QString::number(priority) : QString();
}

QString priorityName(int priority)
{
    switch (priority) {
    case 1: return QObject::tr("Urgent");
    case 2: return QObject::tr("High");
    case 3: return QObject::tr("Medium");
    case 4: return QObject::tr("Low");
    default: return QObject::tr("None");
    }
}

int priorityOf(const QString &name)
{
    static const QRegularExpression level(uR"(^@?[pP]([1-4])$)"_s);
    const QRegularExpressionMatch m = level.match(name.trimmed());
    return m.hasMatch() ? m.captured(1).toInt() : 0;
}

QString withoutMarkers(const QString &text, const QList<TaskMarker::Kind> &kinds)
{
    QString out = text;
    const QList<TaskMarker> found = taskMarkers(text);
    for (auto it = found.crbegin(); it != found.crend(); ++it) {
        if (!kinds.contains(it->kind))
            continue;
        // With the space before it, or after it when it starts the text.
        qsizetype start = it->start;
        qsizetype end = it->end;
        while (start > 0 && out.at(start - 1).isSpace())
            --start;
        if (start == 0 || out.first(start).trimmed().isEmpty()) {
            start = it->start;
            while (end < out.size() && out.at(end).isSpace())
                ++end;
        }
        out.remove(start, end - start);
    }
    return out;
}

QDate nextOccurrence(const QString &rule, const QDate &from)
{
    static const QRegularExpression every(
        uR"(^every\s+(?:(other|\d+)\s+)?(day|week|month|year|weekday|monday|tuesday|wednesday|thursday|friday|saturday|sunday)s?(?:\s+on\s+(monday|tuesday|wednesday|thursday|friday|saturday|sunday))?$)"_s,
        QRegularExpression::CaseInsensitiveOption);
    static const QStringList weekdays = {u"monday"_s, u"tuesday"_s,  u"wednesday"_s, u"thursday"_s,
                                         u"friday"_s, u"saturday"_s, u"sunday"_s};
    const QRegularExpressionMatch m = every.match(rule.simplified());
    if (!m.hasMatch() || !from.isValid())
        return {};
    const QString count = m.captured(1).toLower();
    // A count beyond any sensible repeat is a rule Jot does not read, rather than days too far
    // off to count.
    bool ok = true;
    const qint64 n = count.isEmpty() ? 1 : count == u"other" ? 2 : count.toLongLong(&ok);
    if (!ok || n < 1 || n > 1000)
        return {};
    const QString step = m.captured(2).toLower();
    const QString on = m.captured(3).toLower();
    // The next `day` of the week after `after`.
    const auto nextOn = [](const QDate &after, int day) {
        return after.addDays((day - after.dayOfWeek() + 6) % 7 + 1);
    };
    if (!on.isEmpty()) {
        // Every N weeks on a day: that day, N weeks on.
        if (step != u"week")
            return {};
        return nextOn(from.addDays(7 * (n - 1)), int(weekdays.indexOf(on)) + 1);
    }
    if (step == u"day")
        return from.addDays(n);
    if (step == u"week")
        return from.addDays(7 * n);
    if (step == u"month")
        return from.addMonths(int(n));
    if (step == u"year")
        return from.addYears(int(n));
    if (step == u"weekday") {
        QDate next = from.addDays(1);
        while (next.dayOfWeek() > 5)
            next = next.addDays(1);
        return next;
    }
    return nextOn(from, int(weekdays.indexOf(step)) + 1);
}

QString completedTaskLine(const QString &line, const QDate &today)
{
    static const QRegularExpression box(
        uR"(^(\s*(?:[-*+]|\d+[.)])\s+\[)[ xX](\]\s*)(.*?)(\r?)$)"_s);
    const QRegularExpressionMatch m = box.match(line);
    if (!m.hasMatch())
        return line;
    const QString text = m.captured(3);
    const QString ending = m.captured(4);
    // Ticked with the day it was done, in place of any it said before.
    QString words = withoutMarkers(text, {TaskMarker::Done});
    while (words.endsWith(u' '))
        words.chop(1);
    const QString completed =
        m.captured(1) + u'x' + m.captured(2) + words + u' ' + doneMarker(today);
    const TaskLine parts = readTaskLine(text);
    const QDate was = QDate::fromString(parts.due.left(10), Qt::ISODate);
    QDate next = nextOccurrence(parts.repeat, was);
    // Done late, it comes round again after today rather than on a day already gone.
    while (next.isValid() && next <= today)
        next = nextOccurrence(parts.repeat, next);
    if (!next.isValid())
        return completed + ending;
    // The next one is due on its day, at the same time, and carries no done day of its own.
    QString again = text;
    for (const TaskMarker &marker : taskMarkers(text)) {
        if (marker.kind == TaskMarker::Due) {
            again.replace(marker.start, marker.end - marker.start,
                          dueMarker(next.toString(Qt::ISODate) + marker.value.mid(10)));
            break;
        }
    }
    again = withoutMarkers(again, {TaskMarker::Done});
    return m.captured(1) + u' ' + m.captured(2) + again + ending + u'\n' + completed + ending;
}

} // namespace jot
