#include "jot/QuickAdd.h"

#include "jot/FrontMatter.h"
#include "jot/TaskLine.h"
#include "jot/Vault.h"

#include <QHash>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

const QStringList kWeekdays = {u"monday"_s, u"tuesday"_s,  u"wednesday"_s, u"thursday"_s,
                               u"friday"_s, u"saturday"_s, u"sunday"_s};
const QStringList kMonths = {u"jan"_s, u"feb"_s, u"mar"_s, u"apr"_s, u"may"_s, u"jun"_s,
                             u"jul"_s, u"aug"_s, u"sep"_s, u"oct"_s, u"nov"_s, u"dec"_s};

/// The day of the week a name means, 1 for Monday, from its full name or first three letters;
/// 0 for none.
int weekday(const QString &name)
{
    for (qsizetype i = 0; i < kWeekdays.size(); ++i) {
        if (kWeekdays.at(i).startsWith(name.left(3).toLower()) && name.size() >= 3)
            return int(i + 1);
    }
    return 0;
}

int month(const QString &name)
{
    return int(kMonths.indexOf(name.left(3).toLower()) + 1);
}

/// The next `day` of the week after `from`, or `from` itself when `orToday`.
QDate next(const QDate &from, int day, bool orToday = false)
{
    int ahead = (day - from.dayOfWeek() + 7) % 7;
    if (ahead == 0 && !orToday)
        ahead = 7;
    return from.addDays(ahead);
}

/// The next `day` of a month on or after `from`, in the first month that has one.
QDate nextDayOfMonth(const QDate &from, int day)
{
    for (int ahead = 0; ahead < 12; ++ahead) {
        const QDate month = QDate(from.year(), from.month(), 1).addMonths(ahead);
        const QDate found(month.year(), month.month(), day);
        if (found.isValid() && found >= from)
            return found;
    }
    return {};
}

/// Takes the first match of `pattern` out of `text`, leaving a space, and gives it.
QRegularExpressionMatch take(QString &text, const QRegularExpression &pattern)
{
    const QRegularExpressionMatch m = pattern.match(text);
    if (m.hasMatch())
        text.replace(m.capturedStart(), m.capturedLength(), u" "_s);
    return m;
}

QRegularExpression words(const QString &pattern)
{
    return QRegularExpression(uR"((?:^|(?<=\s)))"_s + pattern + uR"((?=\s|$|[,.!?]))"_s,
                              QRegularExpression::CaseInsensitiveOption);
}

} // namespace

QString dueNamedAlone(const QString &words, const QDateTime &now)
{
    const QuickTask read = readQuickTask(words, now);
    return read.title.isEmpty() && read.repeat.isEmpty() && read.priority == 0 ? read.due
                                                                               : QString();
}

QuickTask readQuickTask(const QString &typed, const QDateTime &now)
{
    // Markers typed as they are written, such as @weekly or @due 2026-11-02, say it outright.
    const TaskLine written = readTaskLine(typed);
    if (!written.due.isEmpty() || !written.repeat.isEmpty() || written.priority > 0) {
        QuickTask task = readQuickTask(written.title, now);
        if (written.priority > 0)
            task.priority = written.priority;
        if (!written.repeat.isEmpty()) {
            task.repeat = written.repeat;
            if (task.due.isEmpty())
                task.due = now.date().toString(Qt::ISODate);
        }
        if (!written.due.isEmpty()) {
            task.due = written.due;
            task.dayGiven = true;
        }
        task.line = task.title;
        if (!task.repeat.isEmpty())
            task.line += u' ' + repeatMarker(task.repeat);
        if (!task.due.isEmpty())
            task.line += u' ' + dueMarker(task.due);
        if (task.priority > 0)
            task.line += u' ' + priorityMarker(task.priority);
        return task;
    }
    const QDate today = now.date();
    QString text = u' ' + typed + u' ';
    QDate day;
    QTime time;
    QString repeat;

    // How it repeats, and the first day that gives when no other day is said.
    QDate firstDay;
    static const QRegularExpression every = words(
        uR"(every\s+(other\s+|\d+\s+)?(day|week|month|year|monday|tuesday|wednesday|thursday|friday|saturday|sunday)s?)"_s);
    static const QRegularExpression weekdays = words(uR"(every\s+weekday)"_s);
    static const QRegularExpression often = words(uR"((daily|weekly|monthly|yearly))"_s);
    if (const QRegularExpressionMatch m = take(text, weekdays); m.hasMatch()) {
        repeat = u"every weekday"_s;
        firstDay = today.dayOfWeek() <= 5 ? today : next(today, 1);
    } else if (const QRegularExpressionMatch m = take(text, every); m.hasMatch()) {
        const QString step = m.captured(2).toLower();
        const QString count = m.captured(1).trimmed().toLower();
        const int n = count == u"other" ? 2 : count.isEmpty() ? 1 : count.toInt();
        if (const int wd = weekday(step); wd > 0) {
            // Every other Monday is every two weeks, on Mondays.
            repeat = n > 1 ? u"every %1 weeks on %2"_s.arg(n).arg(step) : u"every %1"_s.arg(step);
            firstDay = next(today, wd, true);
        } else {
            repeat = n > 1 ? u"every %1 %2s"_s.arg(n).arg(step) : u"every %1"_s.arg(step);
            firstDay = today;
        }
    } else if (const QRegularExpressionMatch m = take(text, often); m.hasMatch()) {
        static const QHash<QString, QString> steps = {{u"daily"_s, u"day"_s},
                                                      {u"weekly"_s, u"week"_s},
                                                      {u"monthly"_s, u"month"_s},
                                                      {u"yearly"_s, u"year"_s}};
        repeat = u"every "_s + steps.value(m.captured(1).toLower());
        firstDay = today;
    }

    // The day, said one of several ways, with "on" or "by" before it if it has one.
    const QString lead = uR"((?:(?:on|by)\s+)?)"_s;
    static const QRegularExpression iso = words(lead + uR"((\d{4}-\d{2}-\d{2}))"_s);
    // Month first, as written in the US: 11-14-2026, 11/14/26 or 11/14.
    static const QRegularExpression numeric =
        words(lead + uR"((\d{1,2})[/-](\d{1,2})(?:[/-](\d{4}|\d{2}))?)"_s);
    static const QRegularExpression in =
        words(uR"(in\s+(\d+|a|an|one|two|three)\s+(day|week|month|year)s?)"_s);
    static const QRegularExpression nextOne = words(uR"(next\s+(week|month|year))"_s);
    // Words that could be a task's own, as "sat down" or "3d print", are a day only at the end,
    // with nothing after but a time.
    const QString atEnd =
        uR"((?=\s*(?:(?:at\s+)?(?:\d{1,2}(?::\d{2})?\s*(?:am|pm)|[01]?\d:[0-5]\d|noon|midday))?\s*$))"_s;
    static const QRegularExpression ordinal =
        words(lead + uR"(the\s+(\d{1,2})(?:st|nd|rd|th))"_s + atEnd);
    static const QRegularExpression bareOrdinal =
        words(lead + uR"((\d{1,2})(?:st|nd|rd|th))"_s + atEnd);
    static const QRegularExpression span = words(uR"(in\s+(\d+)\s*([dwmy]))"_s);
    // Not after "every", which says how something repeats rather than when it is due.
    static const QRegularExpression bareSpan = words(uR"((?<!every )(\d+)([dwm]))"_s + atEnd);
    static const QRegularExpression endOf =
        words(lead + uR"((eow|eom|end\s+of\s+(?:the\s+)?(?:week|month)))"_s);
    static const QRegularExpression bareShortNamed =
        words(uR"((?<!every )(mon|tues?|wed|thu|thurs?|fri|sat|sun))"_s + atEnd);
    static const QRegularExpression weekend = words(lead + uR"((?:this\s+)?weekend)"_s);
    static const QRegularExpression relative = words(lead + uR"((today|tod|tomorrow|tmrw?))"_s);
    static const QRegularExpression named = words(
        uR"((?:(?:on|by|next|this)\s+)?(monday|tuesday|wednesday|thursday|friday|saturday|sunday))"_s);
    static const QRegularExpression shortNamed =
        words(uR"((?:on|by|next|this)\s+(mon|tues?|wed|thu|thurs?|fri|sat|sun))"_s);
    static const QRegularExpression monthDay = words(
        lead +
        uR"((jan(?:uary)?|feb(?:ruary)?|mar(?:ch)?|apr(?:il)?|may|june?|july?|aug(?:ust)?|sept?(?:ember)?|oct(?:ober)?|nov(?:ember)?|dec(?:ember)?)\.?\s+(\d{1,2})(?:st|nd|rd|th)?(?:,?\s+(\d{4}))?)"_s);
    static const QRegularExpression dayMonth = words(
        lead +
        uR"((\d{1,2})(?:st|nd|rd|th)?\s+(jan(?:uary)?|feb(?:ruary)?|mar(?:ch)?|apr(?:il)?|may|june?|july?|aug(?:ust)?|sept?(?:ember)?|oct(?:ober)?|nov(?:ember)?|dec(?:ember)?)(?:\s+(\d{4}))?)"_s);
    // What it said before the day was taken out, put back when the words name no real day.
    const QString beforeDay = text;
    if (QRegularExpressionMatch m = take(text, iso); m.hasMatch()) {
        day = QDate::fromString(m.captured(1), Qt::ISODate);
    } else if (m = take(text, numeric); m.hasMatch()) {
        const QString year = m.captured(3);
        day = QDate(year.isEmpty()     ? today.year()
                    : year.size() == 2 ? 2000 + year.toInt()
                                       : year.toInt(),
                    m.captured(1).toInt(), m.captured(2).toInt());
        if (year.isEmpty() && day.isValid() && day < today)
            day = day.addYears(1);
    } else if (m = take(text, in); m.hasMatch()) {
        static const QHash<QString, int> spelled = {
            {u"a"_s, 1}, {u"an"_s, 1}, {u"one"_s, 1}, {u"two"_s, 2}, {u"three"_s, 3}};
        const QString amount = m.captured(1).toLower();
        const int n = spelled.value(amount, amount.toInt());
        const QString unit = m.captured(2).toLower();
        day = unit == u"day"     ? today.addDays(n)
              : unit == u"week"  ? today.addDays(7 * n)
              : unit == u"month" ? today.addMonths(n)
                                 : today.addYears(n);
    } else if (m = take(text, nextOne); m.hasMatch()) {
        const QString unit = m.captured(1).toLower();
        day = unit == u"week"    ? next(today, 1)
              : unit == u"month" ? QDate(today.year(), today.month(), 1).addMonths(1)
                                 : QDate(today.year() + 1, 1, 1);
    } else if (m = take(text, weekend); m.hasMatch()) {
        day = next(today, 6, true);
    } else if (m = take(text, relative); m.hasMatch()) {
        day = m.captured(1).toLower().startsWith(u"to") && m.captured(1).size() <= 5
                  ? today
                  : today.addDays(1);
    } else if ((m = take(text, named)).hasMatch() || (m = take(text, shortNamed)).hasMatch()) {
        day = next(today, weekday(m.captured(1)));
    } else if (m = take(text, monthDay); m.hasMatch()) {
        const int year = m.captured(3).isEmpty() ? today.year() : m.captured(3).toInt();
        day = QDate(year, month(m.captured(1)), m.captured(2).toInt());
        if (m.captured(3).isEmpty() && day.isValid() && day < today)
            day = day.addYears(1);
    } else if (m = take(text, dayMonth); m.hasMatch()) {
        const int year = m.captured(3).isEmpty() ? today.year() : m.captured(3).toInt();
        day = QDate(year, month(m.captured(2)), m.captured(1).toInt());
        if (m.captured(3).isEmpty() && day.isValid() && day < today)
            day = day.addYears(1);
    } else if ((m = take(text, ordinal)).hasMatch() || (m = take(text, bareOrdinal)).hasMatch()) {
        // The 12th is the next one: this month's unless it has gone by.
        day = nextDayOfMonth(today, m.captured(1).toInt());
    } else if ((m = take(text, span)).hasMatch() || (m = take(text, bareSpan)).hasMatch()) {
        const int n = m.captured(1).toInt();
        const QString unit = m.captured(2).toLower();
        day = unit == u"d"   ? today.addDays(n)
              : unit == u"w" ? today.addDays(7 * n)
              : unit == u"m" ? today.addMonths(n)
                             : today.addYears(n);
    } else if (m = take(text, endOf); m.hasMatch()) {
        const QString said = m.captured(1).toLower();
        // The week ends on its last working day, Friday; the month on its last day.
        if (said == u"eow" || said.endsWith(u"week"))
            day = next(today, 5, true);
        else
            day = QDate(today.year(), today.month(), today.daysInMonth());
    } else if (m = take(text, bareShortNamed); m.hasMatch()) {
        day = next(today, weekday(m.captured(1)));
    }

    if (!day.isValid())
        text = beforeDay;

    // The time, as 3pm, 3:30 pm, 15:00, noon or "at 9", put back when it is no real time.
    const QString beforeTime = text;
    static const QRegularExpression twelve =
        words(uR"((?:at\s+)?(\d{1,2})(?::(\d{2}))?\s*(am|pm))"_s);
    static const QRegularExpression twentyFour =
        words(uR"((?:at\s+)?([01]?\d|2[0-3]):([0-5]\d))"_s);
    static const QRegularExpression noon = words(uR"((?:at\s+)?(noon|midday))"_s);
    static const QRegularExpression bare = words(uR"(at\s+(\d{1,2}))"_s);
    if (QRegularExpressionMatch m = take(text, twelve); m.hasMatch()) {
        const int clock = m.captured(1).toInt();
        const int hour = clock % 12 + (m.captured(3).toLower() == u"pm" ? 12 : 0);
        if (clock >= 1 && clock <= 12)
            time = QTime(hour, m.captured(2).toInt());
    } else if (m = take(text, twentyFour); m.hasMatch()) {
        time = QTime(m.captured(1).toInt(), m.captured(2).toInt());
    } else if (m = take(text, noon); m.hasMatch()) {
        time = QTime(12, 0);
    } else if (m = take(text, bare); m.hasMatch()) {
        time = QTime(m.captured(1).toInt(), 0);
    }
    if (!time.isValid())
        text = beforeTime;

    // Its priority, as Todoist writes it: p1 to p4.
    static const QRegularExpression level = words(uR"(p([1-4]))"_s);
    int priority = 0;
    if (const QRegularExpressionMatch m = take(text, level); m.hasMatch())
        priority = m.captured(1).toInt();

    const bool dayGiven = day.isValid();
    if (!day.isValid())
        day = firstDay.isValid() ? firstDay : time.isValid() ? today : QDate();

    QuickTask task;
    task.title = text.simplified();
    task.repeat = repeat;
    task.priority = priority;
    task.dayGiven = dayGiven;
    if (day.isValid())
        task.due = time.isValid()
                       ? u"%1T%2"_s.arg(day.toString(Qt::ISODate), time.toString(u"HH:mm"_s))
                       : day.toString(Qt::ISODate);
    task.line = task.title;
    if (!repeat.isEmpty())
        task.line += u' ' + repeatMarker(repeat);
    if (!task.due.isEmpty())
        task.line += u' ' + dueMarker(task.due);
    if (priority > 0)
        task.line += u' ' + priorityMarker(priority);
    return task;
}

QString withTaskSchedule(const QString &text, const QString &due, const QString &repeat)
{
    // The words with the markers gone; the day and repeat go back as given, then the priority
    // and done day as they were.
    QString line = withoutMarkers(
        text, {TaskMarker::Due, TaskMarker::Repeat, TaskMarker::Done, TaskMarker::Priority});
    while (line.endsWith(u' '))
        line.chop(1);
    const TaskLine was = readTaskLine(text);
    if (!repeat.isEmpty())
        line += u' ' + repeatMarker(repeat);
    if (!due.isEmpty())
        line += u' ' + dueMarker(due);
    if (was.priority > 0)
        line += u' ' + priorityMarker(was.priority);
    // A done day, or done with none said.
    const bool done = std::ranges::any_of(taskMarkers(text), [](const TaskMarker &marker) {
        return marker.kind == TaskMarker::Done;
    });
    if (!was.done.isEmpty())
        line += u' ' + doneMarker(QDate::fromString(was.done, Qt::ISODate));
    else if (done)
        line += u" @done"_s;
    return line;
}

std::optional<QString> scheduledFromWords(const QString &text, const QDateTime &now)
{
    const QList<TaskMarker> markers = taskMarkers(text);
    const qsizetype first = markers.isEmpty() ? text.size() : markers.first().start;
    const QuickTask said = readQuickTask(text.first(first), now);
    if (said.due.isEmpty() && said.repeat.isEmpty())
        return std::nullopt;
    const TaskLine was = readTaskLine(text);
    const QString rest = text.sliced(first);
    // A day already written stays unless the words name another; a time said alone goes on it.
    QString due = said.due;
    if (!said.dayGiven && !was.due.isEmpty()) {
        const QString time = said.due.mid(10);
        due = time.isEmpty() ? was.due : was.due.left(10) + time;
    }
    return withTaskSchedule(said.title + u' ' + rest, due.isEmpty() ? was.due : due,
                            said.repeat.isEmpty() ? was.repeat : said.repeat);
}

std::optional<QString> scheduledAs(const QString &text, const QString &when, const QDateTime &now)
{
    if (when.trimmed().isEmpty())
        return withTaskSchedule(text, {}, {});
    const QuickTask said = readQuickTask(when, now);
    // The rule written already stays when it is all the words say besides the day: nothing else
    // is left once it is taken out and the day read.
    const QString was = readTaskLine(text).repeat;
    const qsizetype at = was.isEmpty() ? -1 : when.indexOf(was, 0, Qt::CaseInsensitive);
    const QuickTask rest =
        at < 0 ? QuickTask{} : readQuickTask(QString(when).remove(at, was.size()), now);
    const bool kept = at >= 0 && rest.title.isEmpty() && rest.repeat.isEmpty();
    const QString repeat = kept ? was : said.repeat;
    if (said.due.isEmpty() && repeat.isEmpty())
        return std::nullopt;
    return withTaskSchedule(text, said.due, repeat);
}

QString inboxPath(const Vault &vault)
{
    QString ignored;
    const std::optional<QString> settings =
        notefile::read(vault.absolute(u".jot/vault.yaml"_s), &ignored);
    QString path =
        settings ? readFrontMatter(*settings).value(u"inbox"_s).toString().trimmed() : QString();
    // Only a note inside the vault, and not in Jot's own folder.
    if (path.isEmpty() || path.startsWith(u'/') || path.startsWith(u'.') || path.contains(u"../"_s))
        path = u"Inbox.md"_s;
    return path.endsWith(u".md"_s) ? path : path + u".md"_s;
}

QString withTaskAdded(const QString &note, const QString &line)
{
    const QString newline = note.contains(u"\r\n"_s) ? u"\r\n"_s : u"\n"_s;
    QString text = note;
    if (!text.isEmpty() && !text.endsWith(u'\n'))
        text += newline;
    return text + u"- [ ] "_s + line + newline;
}

} // namespace jot
