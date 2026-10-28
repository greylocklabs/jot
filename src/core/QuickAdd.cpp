#include "jot/QuickAdd.h"

#include "jot/FrontMatter.h"
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

QuickTask readQuickTask(const QString &typed, const QDateTime &now)
{
    const QDate today = now.date();
    QString text = u' ' + typed + u' ';
    QDate day;
    QTime time;
    QString repeat;

    // How it repeats, and the first day that gives when no other day is said.
    QDate firstDay;
    static const QRegularExpression every = words(
        uR"(every\s+(other\s+|\d+\s+)?(day|week|month|year|weekday|monday|tuesday|wednesday|thursday|friday|saturday|sunday)s?)"_s);
    static const QRegularExpression often = words(uR"((daily|weekly|monthly|yearly))"_s);
    if (const QRegularExpressionMatch m = take(text, every); m.hasMatch()) {
        const QString step = m.captured(2).toLower();
        const QString count = m.captured(1).trimmed().toLower();
        const int n = count == u"other" ? 2 : count.isEmpty() ? 1 : count.toInt();
        if (const int wd = weekday(step); wd > 0) {
            repeat = u"every %1"_s.arg(step);
            firstDay = next(today, wd, true);
        } else if (step == u"weekday") {
            repeat = u"every weekday"_s;
            firstDay = today.dayOfWeek() <= 5 ? today : next(today, 1);
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
    static const QRegularExpression in =
        words(uR"(in\s+(\d+|a|an|one|two|three)\s+(day|week|month)s?)"_s);
    static const QRegularExpression nextOne = words(uR"(next\s+(week|month))"_s);
    static const QRegularExpression weekend = words(lead + uR"((?:this\s+)?weekend)"_s);
    static const QRegularExpression relative = words(lead + uR"((today|tod|tomorrow|tmrw?))"_s);
    static const QRegularExpression named = words(
        uR"((?:(?:on|by|next|this)\s+)?(monday|tuesday|wednesday|thursday|friday|saturday|sunday))"_s);
    static const QRegularExpression shortNamed =
        words(uR"((?:on|by|next|this)\s+(mon|tues?|wed|thu|thurs?|fri|sat|sun))"_s);
    static const QRegularExpression monthDay = words(
        lead +
        uR"((jan|feb|mar|apr|may|jun|jul|aug|sep|sept|oct|nov|dec)[a-z]*\.?\s+(\d{1,2})(?:st|nd|rd|th)?(?:,?\s+(\d{4}))?)"_s);
    static const QRegularExpression dayMonth = words(
        lead +
        uR"((\d{1,2})(?:st|nd|rd|th)?\s+(jan|feb|mar|apr|may|jun|jul|aug|sep|sept|oct|nov|dec)[a-z]*(?:\s+(\d{4}))?)"_s);
    if (QRegularExpressionMatch m = take(text, iso); m.hasMatch()) {
        day = QDate::fromString(m.captured(1), Qt::ISODate);
    } else if (m = take(text, in); m.hasMatch()) {
        static const QHash<QString, int> spelled = {
            {u"a"_s, 1}, {u"an"_s, 1}, {u"one"_s, 1}, {u"two"_s, 2}, {u"three"_s, 3}};
        const QString amount = m.captured(1).toLower();
        const int n = spelled.value(amount, amount.toInt());
        const QString unit = m.captured(2).toLower();
        day = unit == u"day"    ? today.addDays(n)
              : unit == u"week" ? today.addDays(7 * n)
                                : today.addMonths(n);
    } else if (m = take(text, nextOne); m.hasMatch()) {
        day = m.captured(1).toLower() == u"week"
                  ? next(today, 1)
                  : QDate(today.year(), today.month(), 1).addMonths(1);
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
    }

    // The time, as 3pm, 3:30 pm, 15:00, noon or "at 9".
    static const QRegularExpression twelve =
        words(uR"((?:at\s+)?(\d{1,2})(?::(\d{2}))?\s*(am|pm))"_s);
    static const QRegularExpression twentyFour =
        words(uR"((?:at\s+)?([01]?\d|2[0-3]):([0-5]\d))"_s);
    static const QRegularExpression noon = words(uR"((?:at\s+)?(noon|midday))"_s);
    static const QRegularExpression bare = words(uR"(at\s+(\d{1,2}))"_s);
    if (QRegularExpressionMatch m = take(text, twelve); m.hasMatch()) {
        int hour = m.captured(1).toInt() % 12;
        if (m.captured(3).toLower() == u"pm")
            hour += 12;
        time = QTime(hour, m.captured(2).toInt());
    } else if (m = take(text, twentyFour); m.hasMatch()) {
        time = QTime(m.captured(1).toInt(), m.captured(2).toInt());
    } else if (m = take(text, noon); m.hasMatch()) {
        time = QTime(12, 0);
    } else if (m = take(text, bare); m.hasMatch() && m.captured(1).toInt() < 24) {
        time = QTime(m.captured(1).toInt(), 0);
    }

    if (!day.isValid())
        day = firstDay.isValid() ? firstDay : time.isValid() ? today : QDate();

    QuickTask task;
    task.title = text.simplified();
    task.repeat = repeat;
    if (day.isValid())
        task.due = time.isValid()
                       ? u"%1T%2"_s.arg(day.toString(Qt::ISODate), time.toString(u"HH:mm"_s))
                       : day.toString(Qt::ISODate);
    task.line = task.title;
    if (!repeat.isEmpty())
        task.line += u" 🔁 "_s + repeat;
    if (day.isValid())
        task.line += u" 📅 "_s + day.toString(Qt::ISODate) +
                     (time.isValid() ? u' ' + time.toString(u"HH:mm"_s) : QString());
    return task;
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
