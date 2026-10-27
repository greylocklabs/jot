#include "EmojiList.h"

#include <QFile>
#include <QRegularExpression>
#include <QVariantMap>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

struct Emoji
{
    QString emoji;
    QString name;
    QStringList words;
};

const QList<Emoji> &all()
{
    static const QList<Emoji> list = [] {
        QList<Emoji> read;
        QFile file(u":/jot/emoji.txt"_s);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return read;
        for (const QString &line : QString::fromUtf8(file.readAll()).split(u'\n')) {
            const qsizetype tab = line.indexOf(u'\t');
            if (line.startsWith(u'#') || tab < 0)
                continue;
            const QString name = line.sliced(tab + 1);
            read.append({line.first(tab), name,
                         name.split(QRegularExpression(u"[^\\w]+"_s), Qt::SkipEmptyParts)});
        }
        return read;
    }();
    return list;
}

} // namespace

QVariantList EmojiList::matching(const QString &query)
{
    const QStringList wanted =
        query.toCaseFolded().split(QRegularExpression(u"[^\\w]+"_s), Qt::SkipEmptyParts);
    QVariantList found;
    for (const Emoji &each : all()) {
        const bool matches = std::ranges::all_of(wanted, [&each](const QString &word) {
            return std::ranges::any_of(each.words,
                                       [&word](const QString &w) { return w.startsWith(word); });
        });
        if (matches)
            found.append(QVariantMap{{u"emoji"_s, each.emoji}, {u"name"_s, each.name}});
    }
    return found;
}

} // namespace jot
