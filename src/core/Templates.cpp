#include "jot/Templates.h"

#include "jot/FrontMatter.h"
#include "jot/Vault.h"

#include <QHash>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// A notebook named by `key` in `.jot/vault.yaml`, or `fallback`. Only one inside the vault
/// and outside Jot's own folder counts.
QString folderSetting(const Vault &vault, const QString &key, const QString &fallback)
{
    QString ignored;
    const std::optional<QString> settings =
        notefile::read(vault.absolute(u".jot/vault.yaml"_s), &ignored);
    QString folder =
        settings ? readFrontMatter(*settings).value(key).toString().trimmed() : QString();
    while (folder.endsWith(u'/'))
        folder.chop(1);
    if (folder.isEmpty() || folder.startsWith(u'/') || folder.startsWith(u'.') ||
        folder.contains(u"../"_s))
        return fallback;
    return folder;
}

/// A date format as Obsidian and moment.js write it, such as `dddd D MMMM YYYY`, as Qt's.
QString qtFormat(const QString &format)
{
    // Text in square brackets is kept as it is, without them.
    static const QRegularExpression token(
        uR"(\[[^\]]*\]|YYYY|YY|MMMM|MMM|MM|M|DD|D|dddd|ddd|HH|H|hh|h|mm|m|ss|s|A|a)"_s);
    static const QHash<QString, QString> qt{
        {u"YYYY"_s, u"yyyy"_s}, {u"YY"_s, u"yy"_s},   {u"MMMM"_s, u"MMMM"_s}, {u"MMM"_s, u"MMM"_s},
        {u"MM"_s, u"MM"_s},     {u"M"_s, u"M"_s},     {u"DD"_s, u"dd"_s},     {u"D"_s, u"d"_s},
        {u"dddd"_s, u"dddd"_s}, {u"ddd"_s, u"ddd"_s}, {u"HH"_s, u"HH"_s},     {u"H"_s, u"H"_s},
        {u"hh"_s, u"hh"_s},     {u"h"_s, u"h"_s},     {u"mm"_s, u"mm"_s},     {u"m"_s, u"m"_s},
        {u"ss"_s, u"ss"_s},     {u"s"_s, u"s"_s},     {u"A"_s, u"AP"_s},      {u"a"_s, u"ap"_s}};
    // Text between the tokens is quoted in one run, since two quoted runs side by side read
    // as a quote mark.
    QString out;
    QString text;
    const auto flush = [&] {
        if (!text.isEmpty())
            out += u"'"_s + text.replace(u"'"_s, u"''"_s) + u"'"_s;
        text.clear();
    };
    qsizetype at = 0;
    for (const QRegularExpressionMatch &m : token.globalMatch(format)) {
        text += QStringView(format).sliced(at, m.capturedStart() - at);
        if (m.captured().startsWith(u'[')) {
            text += m.capturedView().sliced(1).chopped(1);
        } else {
            flush();
            out += qt.value(m.captured());
        }
        at = m.capturedEnd();
    }
    text += QStringView(format).sliced(at);
    flush();
    return out;
}

/// The key a front matter line starts, or nothing for a line that carries on the one above.
QString keyOf(const QString &line)
{
    if (line.isEmpty() || line.front().isSpace() || line.startsWith(u'-') || line.startsWith(u'#'))
        return {};
    const qsizetype colon = line.indexOf(u':');
    return colon > 0 ? line.first(colon).trimmed() : QString();
}

} // namespace

QString templatesFolder(const Vault &vault)
{
    return folderSetting(vault, u"templates"_s, u"Templates"_s);
}

QString dailyFolder(const Vault &vault)
{
    return folderSetting(vault, u"daily"_s, u"Daily"_s);
}

QString dailyNotePath(const Vault &vault, QDate day)
{
    return dailyFolder(vault) + u'/' + day.toString(Qt::ISODate) + u".md"_s;
}

QString dailyTemplatePath(const Vault &vault)
{
    return templatesFolder(vault) + u"/Daily.md"_s;
}

QString fillTemplate(const QString &text, const QString &title, const QDateTime &now)
{
    static const QRegularExpression variable(uR"(\{\{\s*(title|date|time)(?::([^}]*))?\s*\}\})"_s);
    QString out;
    qsizetype at = 0;
    for (const QRegularExpressionMatch &m : variable.globalMatch(text)) {
        out += QStringView(text).sliced(at, m.capturedStart() - at);
        const QString name = m.captured(1);
        const QString format = m.captured(2).trimmed();
        if (name == u"title")
            out += title;
        else if (!format.isEmpty())
            out += now.toString(qtFormat(format));
        else
            out += now.toString(name == u"date" ? u"yyyy-MM-dd"_s : u"HH:mm"_s);
        at = m.capturedEnd();
    }
    return out + text.mid(at);
}

QString noteFromTemplate(const QString &made, const QString &filled)
{
    const NoteParts mine = splitFrontMatter(made);
    const NoteParts theirs = splitFrontMatter(filled);
    if (!theirs.hasFrontMatter)
        return made + filled;
    QStringList kept = mine.frontMatter.toString().split(u'\n');
    if (!kept.isEmpty() && kept.last().isEmpty())
        kept.removeLast();
    QStringList taken;
    for (const QString &line : std::as_const(kept)) {
        if (const QString key = keyOf(line); !key.isEmpty())
            taken << key;
    }
    // The new note's own keys, such as when it was made, win over the template's.
    bool skipping = false;
    for (const QString &line : theirs.frontMatter.toString().split(u'\n')) {
        if (const QString key = keyOf(line); !key.isEmpty())
            skipping = taken.contains(key);
        if (!skipping && !line.isEmpty())
            kept << line;
    }
    return u"---\n"_s + kept.join(u'\n') + u"\n---\n"_s + mine.body.toString() +
           theirs.body.toString();
}

} // namespace jot
