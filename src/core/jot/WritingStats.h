#pragma once

#include <QDate>
#include <QMap>
#include <QString>
#include <QStringView>

namespace jot {

/// How much text there is: words, as runs of letters and digits that may hold an apostrophe
/// or hyphen, and characters, spaces included but not line breaks.
struct TextCount
{
    int words = 0;
    int characters = 0;
};

[[nodiscard]] TextCount countText(QStringView text);

/// Words written each day, kept in a vault's `.jot/stats/` folder: one file for each computer,
/// `<computer>.yaml`, of `YYYY-MM-DD: words` lines, so syncing never has two computers write
/// the same file. A day's words are what its edits added, less what they took away.
class WritingStats
{
public:
    /// Reads the files in `folder`; `computer` names the one this computer writes.
    WritingStats(QString folder, QString computer);

    /// Adds `words`, which may be fewer than none, to this computer's count for `day`.
    bool add(QDate day, int words, QString *error);
    /// The words written on `day`, on every computer.
    [[nodiscard]] int wordsOn(QDate day) const;
    /// The days in a row, up to `today`, with at least `goal` words, or any for a goal of 0.
    /// Today counts once it gets there; until then the run up to yesterday still stands.
    [[nodiscard]] int streak(QDate today, int goal) const;

private:
    void read();

    QString m_folder;
    QString m_computer;
    /// Each computer's words by day.
    QMap<QString, QMap<QDate, int>> m_days;
};

} // namespace jot
