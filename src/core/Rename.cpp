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

/// The vault path a Markdown link's target names as written, as other tools read it: from the
/// vault's root when it starts with `/`, otherwise from `folder`.
QString literalPath(const QString &raw, const QString &folder)
{
    const QString path = QUrl::fromPercentEncoding(raw.section(u'#', 0, 0).toUtf8());
    if (path.startsWith(u'/'))
        return QDir::cleanPath(path.mid(1));
    return QDir::cleanPath(folder.isEmpty() ? path : folder + u'/' + path);
}

} // namespace

QString retarget(const QString &raw, const QString &source, const QString &to)
{
    const qsizetype hash = raw.indexOf(u'#');
    const QString anchor = hash < 0 ? QString() : raw.mid(hash);
    const QString target = hash < 0 ? raw : raw.left(hash);
    const QString decoded = QUrl::fromPercentEncoding(target.toUtf8());

    const QString folder = folderOf(source);
    // A link from the vault's root stays one.
    QString path = decoded.startsWith(u'/') ? u'/' + to
                   : folder.isEmpty()       ? to
                                            : QDir(u"/"_s + folder).relativeFilePath(u"/"_s + to);
    if (!decoded.endsWith(kNoteSuffix, Qt::CaseInsensitive) && path.endsWith(kNoteSuffix))
        path.chop(kNoteSuffix.size());
    return Vault::escapeLinkPath(path) + anchor;
}

std::optional<QStringList> renameNote(const Vault &vault, VaultIndex &index, const QString &from,
                                      const QString &to, QString *error,
                                      const BeforeWrite &beforeWrite)
{
    // Worked out before the move, while the index still knows where links lead. The note's
    // attachments move with it, so links into them follow too.
    const QString assetsFrom = Vault::assetsFolderFor(from) + u'/';
    const QString assetsTo = Vault::assetsFolderFor(to) + u'/';
    const auto movedTo = [&](const QString &target) {
        if (target == from)
            return to;
        return target.startsWith(assetsFrom) ? assetsTo + target.sliced(assetsFrom.size())
                                             : QString();
    };
    QSet<QString> sources;
    for (const NoteLink &link : index.backlinks(from))
        sources.insert(link.source);
    for (const QString &source : index.sourcesLinkingInto(Vault::assetsFolderFor(from)))
        sources.insert(source);
    sources.remove(from);
    QList<std::pair<QString, QString>> rewrites;
    for (const QString &source : std::as_const(sources)) {
        const std::optional<QString> text = notefile::read(vault.absolute(source), error);
        if (!text)
            return std::nullopt;
        const NoteParts parts = splitFrontMatter(*text);
        const QString body = rewriteLinks(parts.body, [&](const QString &raw) {
            const QString moved = movedTo(index.resolve(raw, source));
            return moved.isEmpty() ? std::nullopt : std::optional(retarget(raw, source, moved));
        });
        rewrites.append({source, text->first(parts.body.data() - text->data()) + body});
    }

    // The note's own paths start from its folder, which is about to change.
    const std::optional<QString> own = notefile::read(vault.absolute(from), error);
    if (!own)
        return std::nullopt;
    // A data file holds no Markdown links, only text that might look like them.
    const NoteParts ownParts =
        Vault::isDataFile(from) ? NoteParts{{}, QStringView(*own), false} : splitFrontMatter(*own);
    const QString ownBody = Vault::isDataFile(from)
                                ? ownParts.body.toString()
                                : rewriteLinks(ownParts.body, [&](const QString &raw) {
                                      const QString target = index.resolve(raw, from);
                                      if (target.isEmpty())
                                          return std::optional<QString>();
                                      const QString moved =
                                          movedTo(target).isEmpty() ? target : movedTo(target);
                                      // Other tools read a Markdown path literally, so it must
                                      // still name the file from the new folder, even where Jot
                                      // would find it by name.
                                      const QString literal = literalPath(raw, folderOf(to));
                                      if (literal == moved || literal + kNoteSuffix == moved)
                                          return std::optional<QString>();
                                      return std::optional(retarget(raw, to, moved));
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
        // A data file's text is the user's data, whatever in it looks like a link.
        if (Vault::isDataFile(source))
            continue;
        const std::optional<QString> text = notefile::read(vault.absolute(source), error);
        if (!text)
            return std::nullopt;
        const QString newSource = after(source);
        const NoteParts parts = splitFrontMatter(*text);
        const QString body = rewriteLinks(parts.body, [&](const QString &raw) {
            const QString target = index.resolve(raw, source);
            if (target.isEmpty() || (!inside(target) && !inside(source)))
                return std::optional<QString>();
            const QString newTarget = after(target);
            // Other tools read a Markdown path literally, so it must name the file from the
            // note's folder after the move.
            const QString literal = literalPath(raw, folderOf(newSource));
            if (literal == newTarget || literal + kNoteSuffix == newTarget)
                return std::optional<QString>();
            return std::optional(retarget(raw, newSource, newTarget));
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
