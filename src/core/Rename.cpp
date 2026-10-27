#include "jot/Rename.h"

#include "jot/FrontMatter.h"
#include "jot/Layout.h"
#include "jot/Logging.h"
#include "jot/NoteScan.h"

#include <QDir>
#include <QHash>
#include <QSet>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

const QString kNoteSuffix = u".md"_s;

QString folderOf(const QString &path)
{
    const qsizetype slash = path.lastIndexOf(u'/');
    return slash < 0 ? QString() : path.left(slash);
}

/// What the layout says about it goes with it. The move stands even when that cannot be
/// written, as the layout is only how things are shown.
void followInLayout(const Vault &vault, const QString &from, const QString &to)
{
    QString why;
    if (!Layout::follow(vault, from, to, &why))
        qCWarning(lcVault) << "could not update" << Layout::kPath << "after moving" << from << why;
}

} // namespace

QString retarget(const QString &raw, bool wiki, const QString &source, const QString &to)
{
    const qsizetype hash = raw.indexOf(u'#');
    const QString anchor = hash < 0 ? QString() : raw.mid(hash);
    const QString target = hash < 0 ? raw : raw.left(hash);
    const QString decoded = QUrl::fromPercentEncoding(target.toUtf8());

    if (wiki) {
        const QString stem = to.endsWith(kNoteSuffix) ? to.chopped(kNoteSuffix.size()) : to;
        // A name stays a name; a path, which was there to tell notes apart, stays a path.
        return (decoded.contains(u'/') ? stem : Vault::titleFromPath(to)) + anchor;
    }
    const QString folder = folderOf(source);
    QString path = folder.isEmpty() ? to : QDir(u"/"_s + folder).relativeFilePath(u"/"_s + to);
    if (!decoded.endsWith(kNoteSuffix, Qt::CaseInsensitive) && path.endsWith(kNoteSuffix))
        path.chop(kNoteSuffix.size());
    return path + anchor;
}

std::optional<QStringList> renameNote(const Vault &vault, VaultIndex &index, const QString &from,
                                      const QString &to, QString *error,
                                      const BeforeWrite &beforeWrite)
{
    // Worked out before the move, while the index still knows where links lead.
    QSet<QString> sources;
    for (const NoteLink &link : index.backlinks(from))
        sources.insert(link.source);
    sources.remove(from);
    QList<std::pair<QString, QString>> rewrites;
    for (const QString &source : std::as_const(sources)) {
        const std::optional<QString> text = notefile::read(vault.absolute(source), error);
        if (!text)
            return std::nullopt;
        const NoteParts parts = splitFrontMatter(*text);
        const QString body = rewriteLinks(parts.body, [&](const QString &raw, bool wiki) {
            return index.resolve(raw, source) == from
                       ? std::optional(retarget(raw, wiki, source, to))
                       : std::nullopt;
        });
        rewrites.append({source, text->first(parts.body.data() - text->data()) + body});
    }

    // The note's own paths start from its folder, which is about to change.
    const std::optional<QString> own = notefile::read(vault.absolute(from), error);
    if (!own)
        return std::nullopt;
    const NoteParts ownParts = splitFrontMatter(*own);
    const QString ownBody = rewriteLinks(ownParts.body, [&](const QString &raw, bool wiki) {
        const QString target = index.resolve(raw, from);
        if (target.isEmpty() || (wiki && target != from))
            return std::optional<QString>();
        const QString moved = target == from ? to : target;
        if (!wiki) {
            // Other tools read a Markdown path literally, so it must still name the file
            // from the new folder, even where Jot would find it by name.
            const QString path = raw.section(u'#', 0, 0);
            const QString decoded = QUrl::fromPercentEncoding(path.toUtf8());
            const QString literal =
                QDir::cleanPath(folderOf(to).isEmpty() ? decoded : folderOf(to) + u'/' + decoded);
            if (literal == moved || literal + kNoteSuffix == moved)
                return std::optional<QString>();
        }
        return std::optional(retarget(raw, wiki, to, moved));
    });
    const QString ownText = own->first(ownParts.body.data() - own->data()) + ownBody;

    if (!vault.moveNote(from, to, error))
        return std::nullopt;
    followInLayout(vault, from, to);
    if (ownText != *own) {
        if (beforeWrite)
            beforeWrite(to, ownText);
        if (!notefile::write(vault.absolute(to), ownText, error))
            return std::nullopt;
    }
    QStringList written;
    for (const auto &[source, text] : std::as_const(rewrites)) {
        if (beforeWrite)
            beforeWrite(source, text);
        if (!notefile::write(vault.absolute(source), text, error))
            return std::nullopt;
        written.append(source);
    }
    index.update(QStringList{from, to} + written);
    written.sort();
    return written;
}

std::optional<QStringList> moveNotebook(const Vault &vault, VaultIndex &index, const QString &from,
                                        const QString &to, QString *error,
                                        const BeforeWrite &beforeWrite)
{
    // Everything inside goes with it, notes and attachments alike.
    const auto inside = [&from](const QString &path) { return path.startsWith(from + u'/'); };
    const auto after = [&](const QString &path) {
        return inside(path) ? to + path.mid(from.size()) : path;
    };
    QStringList notesInside;
    for (const NoteInfo &note : index.notesIn(from))
        notesInside.append(note.path);

    // Every note inside, and every note with a link to anything inside, may need its links
    // redone; worked out while the index still knows where links lead.
    QSet<QString> sources(notesInside.cbegin(), notesInside.cend());
    for (const QString &source : index.sourcesLinkingInto(from))
        sources.insert(source);
    QList<std::pair<QString, QString>> rewrites;
    for (const QString &source : std::as_const(sources)) {
        const std::optional<QString> text = notefile::read(vault.absolute(source), error);
        if (!text)
            return std::nullopt;
        const QString newSource = after(source);
        const NoteParts parts = splitFrontMatter(*text);
        const QString body = rewriteLinks(parts.body, [&](const QString &raw, bool wiki) {
            const QString target = index.resolve(raw, source);
            if (target.isEmpty() || (!inside(target) && !inside(source)))
                return std::optional<QString>();
            const QString newTarget = after(target);
            if (wiki) {
                // A wikilink by name, or by a path that did not change, still reaches it.
                return target == newTarget
                           ? std::optional<QString>()
                           : std::optional(retarget(raw, wiki, newSource, newTarget));
            }
            // Other tools read a Markdown path literally, so it must name the file from the
            // note's folder after the move.
            const QString path = QUrl::fromPercentEncoding(raw.section(u'#', 0, 0).toUtf8());
            const QString folder = folderOf(newSource);
            const QString literal = QDir::cleanPath(folder.isEmpty() ? path : folder + u'/' + path);
            if (literal == newTarget || literal + kNoteSuffix == newTarget)
                return std::optional<QString>();
            return std::optional(retarget(raw, wiki, newSource, newTarget));
        });
        if (body != parts.body)
            rewrites.append({newSource, text->first(parts.body.data() - text->data()) + body});
    }

    if (!vault.moveFolder(from, to, error))
        return std::nullopt;
    followInLayout(vault, from, to);
    QStringList written;
    for (const auto &[path, text] : std::as_const(rewrites)) {
        if (beforeWrite)
            beforeWrite(path, text);
        if (!notefile::write(vault.absolute(path), text, error))
            return std::nullopt;
        written.append(path);
    }
    QStringList changed = written;
    for (const QString &note : std::as_const(notesInside))
        changed << note << after(note);
    index.update(changed);
    written.sort();
    return written;
}

} // namespace jot
