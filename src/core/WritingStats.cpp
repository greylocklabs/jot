#include "jot/WritingStats.h"

#include "jot/Vault.h"

#include <QDir>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace jot {

TextCount countText(QStringView text)
{
    static const QRegularExpression word(uR"([\p{L}\p{N}]+(?:['\x{2019}-][\p{L}\p{N}]+)*)"_s);
    TextCount count;
    for (auto it = word.globalMatchView(text); it.hasNext(); it.next())
        ++count.words;
    count.characters = int(text.size() - text.count(u'\n') - text.count(u'\r'));
    return count;
}

WritingStats::WritingStats(QString folder, QString computer)
    : m_folder(std::move(folder)), m_computer(std::move(computer))
{
    read();
}

void WritingStats::read()
{
    m_days.clear();
    const QDir dir(m_folder);
    for (const QString &name : dir.entryList({u"*.yaml"_s}, QDir::Files)) {
        QString ignored;
        const std::optional<QString> text = notefile::read(dir.filePath(name), &ignored);
        if (!text)
            continue;
        QMap<QDate, int> &days = m_days[name.chopped(5)];
        for (const QString &line : text->split(u'\n')) {
            const qsizetype colon = line.indexOf(u':');
            const QDate day =
                QDate::fromString(line.first(std::max<qsizetype>(colon, 0)).trimmed(), Qt::ISODate);
            bool number = false;
            const int words = line.sliced(colon + 1).trimmed().toInt(&number);
            if (day.isValid() && number)
                days[day] = words;
        }
    }
}

bool WritingStats::add(QDate day, int words, QString *error)
{
    if (words == 0)
        return true;
    // Another computer's file may have come in by sync since.
    read();
    QMap<QDate, int> &days = m_days[m_computer];
    days[day] += words;
    QString text;
    for (auto it = days.cbegin(); it != days.cend(); ++it)
        text += u"%1: %2\n"_s.arg(it.key().toString(Qt::ISODate)).arg(it.value());
    if (!QDir().mkpath(m_folder)) {
        *error = QObject::tr("could not create %1").arg(m_folder);
        return false;
    }
    return notefile::write(QDir(m_folder).filePath(m_computer + u".yaml"_s), text, error);
}

int WritingStats::wordsOn(QDate day) const
{
    int words = 0;
    for (const QMap<QDate, int> &days : m_days)
        words += days.value(day);
    return std::max(words, 0);
}

int WritingStats::streak(QDate today, int goal) const
{
    const auto met = [&](QDate day) {
        const int words = wordsOn(day);
        return goal > 0 ? words >= goal : words > 0;
    };
    QDate day = met(today) ? today : today.addDays(-1);
    int days = 0;
    while (met(day)) {
        ++days;
        day = day.addDays(-1);
    }
    return days;
}

} // namespace jot
