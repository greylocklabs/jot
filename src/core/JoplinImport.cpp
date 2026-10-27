#include "jot/JoplinImport.h"

#include "jot/FrontMatter.h"
#include "jot/Layout.h"
#include "jot/NoteScan.h"
#include "jot/Vault.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QObject>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSet>
#include <QTimeZone>

#include <algorithm>
#include <functional>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

const QString kLedger = u".jot/joplin.yaml"_s;

QString join(const QString &folder, const QString &name)
{
    return folder.isEmpty() ? name : folder + u'/' + name;
}

QString sha(const QString &text)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString iso(const QDateTime &time)
{
    return time.toUTC().toString(Qt::ISODate);
}

/// What earlier imports brought into the vault, by Joplin id, as `.jot/joplin.yaml` keeps it.
struct Ledger
{
    struct Notebook
    {
        QString path;
        /// The Joplin notebook it belongs in, which may not have been imported yet.
        QString parent;
        /// Put at the top because its parent was not imported with it.
        bool orphan = false;
    };
    struct Note
    {
        QString path;
        /// Joplin's update time when it was imported.
        QString updated;
        /// The file's text as the import wrote it.
        QString sha;
    };
    QHash<QString, Notebook> notebooks;
    QHash<QString, Note> notes;
    QHash<QString, QString> attachments;
};

QString unquote(const QString &value)
{
    if (value.size() < 2 || !value.startsWith(u'"') || !value.endsWith(u'"'))
        return value;
    QString out;
    for (qsizetype i = 1; i + 1 < value.size(); ++i) {
        if (value[i] == u'\\' && i + 2 < value.size()) {
            const QChar next = value[++i];
            out += next == u'n' ? u'\n' : next == u't' ? u'\t' : next == u'r' ? u'\r' : next;
        } else {
            out += value[i];
        }
    }
    return out;
}

/// The ledger's three sections, each a map of ids to a value or to keys of their own.
Ledger readLedger(const QString &path)
{
    Ledger ledger;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return ledger;
    static const QRegularExpression line(u"^( *)([^:#]+):(?: (.*))?$"_s);
    QString section;
    QString id;
    for (const QString &raw : QString::fromUtf8(file.readAll()).split(u'\n')) {
        const auto m = line.match(raw);
        if (!m.hasMatch())
            continue;
        const int indent = int(m.captured(1).size());
        const QString key = m.captured(2).trimmed();
        const QString value = unquote(m.captured(3).trimmed());
        if (indent == 0) {
            section = key;
        } else if (indent == 2) {
            id = key;
            if (section == u"attachments"_s)
                ledger.attachments.insert(id, value);
        } else if (section == u"notebooks"_s) {
            auto &notebook = ledger.notebooks[id];
            if (key == u"path"_s)
                notebook.path = value;
            else if (key == u"parent"_s)
                notebook.parent = value;
            else if (key == u"orphan"_s)
                notebook.orphan = value == u"true"_s;
        } else if (section == u"notes"_s) {
            auto &note = ledger.notes[id];
            if (key == u"path"_s)
                note.path = value;
            else if (key == u"updated"_s)
                note.updated = value;
            else if (key == u"sha"_s)
                note.sha = value;
        }
    }
    return ledger;
}

QString ledgerText(const Ledger &ledger)
{
    const auto sorted = [](auto keys) {
        std::sort(keys.begin(), keys.end());
        return keys;
    };
    QString out =
        u"# Written by Jot's Joplin import, so a later import knows what came from where.\n"_s;
    out += u"notebooks:\n"_s;
    for (const QString &id : sorted(ledger.notebooks.keys())) {
        const auto &n = ledger.notebooks[id];
        out += u"  %1:\n    path: %2\n"_s.arg(id, yamlScalar(n.path));
        if (!n.parent.isEmpty())
            out += u"    parent: %1\n"_s.arg(n.parent);
        if (n.orphan)
            out += u"    orphan: true\n"_s;
    }
    out += u"notes:\n"_s;
    for (const QString &id : sorted(ledger.notes.keys())) {
        const auto &n = ledger.notes[id];
        out += u"  %1:\n    path: %2\n    updated: %3\n    sha: %4\n"_s.arg(id, yamlScalar(n.path),
                                                                            n.updated, n.sha);
    }
    out += u"attachments:\n"_s;
    for (const QString &id : sorted(ledger.attachments.keys()))
        out += u"  %1: %2\n"_s.arg(id, yamlScalar(ledger.attachments[id]));
    return out;
}

/// A Joplin `:/id` link target's id, or empty for any other target.
QString joplinTarget(const QString &target)
{
    static const QRegularExpression id(u"^:/([0-9a-f]{32})(?:#.*)?$"_s);
    return id.match(target).captured(1);
}

} // namespace

int JoplinImport::count(Outcome outcome) const
{
    return int(std::count_if(notes.cbegin(), notes.cend(),
                             [&](const Note &n) { return n.outcome == outcome; }));
}

std::optional<JoplinImport> importJoplin(const Vault &vault, const JoplinExport &from,
                                         const QString &folder, bool dryRun, QString *error)
{
    if (!dryRun && !vault.writable()) {
        *error = QObject::tr("this vault is from a newer Jot, so this one only reads it");
        return std::nullopt;
    }
    Ledger ledger = readLedger(vault.absolute(kLedger));
    JoplinImport result;
    result.skipped = from.skipped;
    // Paths this import has taken, so a dry run numbers names as a real one would.
    QSet<QString> taken;
    const auto exists = [&](const QString &path) {
        return taken.contains(path) || QFileInfo::exists(vault.absolute(path));
    };
    const auto freeName = [&](const QString &dir, const QString &base, const QString &suffix) {
        QString path = join(dir, base + suffix);
        for (int n = 2; exists(path); ++n)
            path = join(dir, u"%1 %2"_s.arg(base).arg(n) + suffix);
        taken.insert(path);
        return path;
    };
    // The ledger has to cover whatever is on disk, or the next import copies those notes again.
    const auto saveLedger = [&](QString *why) {
        if (dryRun)
            return true;
        QDir().mkpath(vault.absolute(u".jot"_s));
        return notefile::write(vault.absolute(kLedger), ledgerText(ledger), why);
    };
    const auto fail = [&](const QString &why) -> std::optional<JoplinImport> {
        QString ignored;
        saveLedger(&ignored);
        *error = why;
        return std::nullopt;
    };

    // Notebooks, parents first. One whose parent is neither here nor imported before goes at
    // the top, and moves under its parent when a later import brings that.
    QHash<QString, const JoplinExport::Notebook *> notebooks;
    for (const auto &notebook : from.notebooks)
        notebooks.insert(notebook.id, &notebook);
    QHash<QString, QString> folderOf;
    // Each notebook's emoji, by the folder it became.
    QHash<QString, QString> emojiOf;
    QSet<QString> dropped;
    // Being placed, so a notebook that is its own ancestor, as a broken export may say, stops
    // at the top instead of recursing for ever.
    QSet<QString> placing;
    QString unmade;
    std::function<QString(const QString &)> placeNotebook = [&](const QString &id) -> QString {
        if (folderOf.contains(id))
            return folderOf[id];
        if (placing.contains(id))
            return folder;
        placing.insert(id);
        const auto placed = qScopeGuard([&] { placing.remove(id); });
        const auto *notebook = notebooks.value(id);
        const bool known = ledger.notebooks.contains(id);
        if (!notebook && !known)
            return folderOf[id] = folder;
        if (notebook && notebook->deleted) {
            dropped.insert(id);
            return folderOf[id] = folder;
        }
        const QString parentId = notebook ? notebook->parentId : ledger.notebooks[id].parent;
        const bool parentKnown = !parentId.isEmpty() && (notebooks.contains(parentId) ||
                                                         ledger.notebooks.contains(parentId));
        const QString parent = parentKnown ? placeNotebook(parentId) : folder;
        if (known) {
            auto &entry = ledger.notebooks[id];
            if (entry.orphan && parentKnown) {
                const QString moved = join(parent, entry.path.section(u'/', -1));
                QString why;
                if (QFileInfo(vault.absolute(entry.path)).isDir() && !exists(moved) &&
                    (dryRun || vault.moveFolder(entry.path, moved, &why))) {
                    // Everything recorded under the old place is under the new one now.
                    const auto rebase = [&](QString &path) {
                        if (path == entry.path || path.startsWith(entry.path + u'/'))
                            path = moved + path.sliced(entry.path.size());
                    };
                    for (auto &n : ledger.notes)
                        rebase(n.path);
                    for (auto &other : ledger.notebooks) {
                        if (&other != &entry)
                            rebase(other.path);
                    }
                    for (auto &a : ledger.attachments)
                        rebase(a);
                    entry.path = moved;
                    entry.orphan = false;
                }
            }
            if (QFileInfo(vault.absolute(entry.path)).isDir() || taken.contains(entry.path))
                return folderOf[id] = entry.path;
        }
        // A folder of the same name is merged with rather than numbered. One known only from an
        // earlier import, whose folder has gone, comes back under the name it had.
        const QString name = notebook ? Vault::fileNameFor(notebook->title)
                                      : ledger.notebooks[id].path.section(u'/', -1);
        const QString path = join(parent, name);
        if (!QFileInfo(vault.absolute(path)).isDir() && !taken.contains(path)) {
            if (!dryRun && !QDir().mkpath(vault.absolute(path)))
                unmade = path;
            result.notebooks.append(path);
            taken.insert(path);
        }
        ledger.notebooks[id] = {path, parentId, !parentId.isEmpty() && !parentKnown};
        return folderOf[id] = path;
    };
    for (const auto &notebook : from.notebooks) {
        const QString path = placeNotebook(notebook.id);
        if (!notebook.emoji.isEmpty() && !dropped.contains(notebook.id) && path != folder)
            emojiOf.insert(path, notebook.emoji);
    }
    // Notebooks imported before without their parent, whose parent this import brings.
    for (const QString &id : ledger.notebooks.keys()) {
        const auto &entry = ledger.notebooks[id];
        if (entry.orphan &&
            (notebooks.contains(entry.parent) || ledger.notebooks.contains(entry.parent)))
            placeNotebook(id);
    }
    if (!unmade.isEmpty())
        return fail(QObject::tr("could not make the folder %1").arg(unmade));

    // Where each note goes, and whether it is written at all. A note imported before is found
    // at its recorded path, or elsewhere by its unchanged text when it was moved in Jot.
    QHash<QString, QString> textAt;
    const auto textOf = [&](const QString &path) -> QString {
        if (!textAt.contains(path)) {
            QString ignored;
            textAt.insert(path, notefile::read(vault.absolute(path), &ignored).value_or(QString()));
        }
        return textAt[path];
    };
    QMultiHash<QString, QString> pathsBySha;
    bool scanned = false;
    // The one file with the text `wanted` that no other imported note is recorded at. Two such
    // files, as two empty notes would be, give none rather than a guess that could overwrite one.
    const auto findMoved = [&](const QString &id, const QString &wanted) -> QString {
        if (!scanned) {
            for (const QString &path : vault.notes())
                pathsBySha.insert(sha(textOf(path)), path);
            scanned = true;
        }
        QStringList candidates;
        for (const QString &path : pathsBySha.values(wanted)) {
            const bool claimed = std::any_of(ledger.notes.cbegin(), ledger.notes.cend(),
                                             [&](const auto &n) { return n.path == path; }) &&
                                 ledger.notes.value(id).path != path;
            if (!claimed)
                candidates.append(path);
        }
        return candidates.size() == 1 ? candidates.first() : QString();
    };

    struct Planned
    {
        const JoplinExport::Note *note;
        QString title;
        QString path;
    };
    QList<Planned> writes;
    QHash<QString, QString> notePath;
    int trashed = 0;
    for (const auto &note : from.notes) {
        if (note.deleted || dropped.contains(note.notebookId)) {
            ++trashed;
            continue;
        }
        QString title =
            note.title.trimmed().isEmpty() ? QObject::tr("Untitled") : note.title.trimmed();
        if (note.conflict)
            title = QObject::tr("%1 (conflict)").arg(title);
        JoplinImport::Note line{note.id, title, {}, JoplinImport::Outcome::Created};
        if (ledger.notes.contains(note.id)) {
            const auto &before = ledger.notes[note.id];
            QString path = QFileInfo::exists(vault.absolute(before.path))
                               ? before.path
                               : findMoved(note.id, before.sha);
            line.path = path.isEmpty() ? before.path : path;
            if (path.isEmpty())
                line.outcome = JoplinImport::Outcome::Removed;
            else if (before.updated == iso(note.updated))
                line.outcome = JoplinImport::Outcome::Unchanged;
            else if (sha(textOf(path)) == before.sha)
                line.outcome = JoplinImport::Outcome::Updated;
            else
                line.outcome = JoplinImport::Outcome::ChangedInJot;
            if (!path.isEmpty()) {
                ledger.notes[note.id].path = path;
                taken.insert(path);
            }
        } else {
            line.path = freeName(folderOf.value(note.notebookId, folder), Vault::fileNameFor(title),
                                 u".md"_s);
        }
        notePath.insert(note.id, line.path);
        if (line.outcome == JoplinImport::Outcome::Created ||
            line.outcome == JoplinImport::Outcome::Updated)
            writes.append({&note, title, line.path});
        result.notes.append(line);
    }
    for (auto it = ledger.notes.cbegin(); it != ledger.notes.cend(); ++it) {
        if (!notePath.contains(it.key()))
            notePath.insert(it.key(), it->path);
    }

    QHash<QString, const JoplinExport::Resource *> resources;
    for (const auto &resource : from.resources)
        resources.insert(resource.id, &resource);
    QHash<QString, QString> tagTitles;
    for (const auto &tag : from.tags)
        tagTitles.insert(tag.id, tag.title);

    int missingFiles = 0;
    int unwritten = 0;
    // The vault path an attachment ends up at, copying it beside the first note that links it.
    const auto attachmentFor = [&](const QString &id, const QString &note) -> QString {
        if (ledger.attachments.contains(id) &&
            (taken.contains(ledger.attachments[id]) ||
             QFileInfo::exists(vault.absolute(ledger.attachments[id]))))
            return ledger.attachments[id];
        const auto *resource = resources.value(id);
        if (!resource)
            return {};
        if (resource->data.isNull()) {
            ++missingFiles;
            return {};
        }
        QString name =
            Vault::fileNameFor(resource->fileName.isEmpty() ? resource->title : resource->fileName);
        if (!resource->extension.isEmpty() &&
            !name.endsWith(u'.' + resource->extension, Qt::CaseInsensitive))
            name += u'.' + resource->extension;
        const QFileInfo info(name);
        const QString suffix = info.suffix().isEmpty() ? QString() : u'.' + info.suffix();
        const QString path =
            freeName(Vault::assetsFolderFor(note), info.completeBaseName(), suffix);
        if (!dryRun) {
            // The name is free, so the vault keeps it as planned.
            QString why;
            if (!vault.addAttachment(note, QFileInfo(path).fileName(), resource->data, &why)) {
                ++unwritten;
                return {};
            }
        }
        ++result.attachments;
        ledger.attachments.insert(id, path);
        return path;
    };

    int written = 0;
    for (const Planned &write : writes) {
        const auto &note = *write.note;
        const auto target = [&](const QString &id) -> std::optional<QString> {
            if (notePath.contains(id))
                return Vault::linkTarget(write.path, notePath[id]);
            const QString attachment = attachmentFor(id, write.path);
            if (attachment.isEmpty())
                return std::nullopt;
            return Vault::linkTarget(write.path, attachment);
        };
        QString body =
            rewriteLinks(note.body, [&](const QString &raw, bool wiki) -> std::optional<QString> {
                const QString id = joplinTarget(raw);
                if (wiki || id.isEmpty())
                    return std::nullopt;
                return target(id);
            });
        if (note.html) {
            // Joplin's HTML notes link with src and href attributes rather than Markdown.
            static const QRegularExpression attribute(
                u"((?:src|href)=[\"']):/([0-9a-f]{32})([\"'])"_s);
            QString out;
            qsizetype last = 0;
            for (const auto &m : attribute.globalMatch(body)) {
                out += body.mid(last, m.capturedStart() - last);
                const auto to = target(m.captured(2));
                out += to ? m.captured(1) + *to + m.captured(3) : m.captured();
                last = m.capturedEnd();
            }
            body = out + body.mid(last);
        }

        QString text = body.endsWith(u'\n') || body.isEmpty() ? body : body + u'\n';
        const auto set = [&](const QString &key, const QVariant &value) {
            text = setFrontMatterKey(text, key, value);
        };
        // Only when the file name cannot hold the title, as for any note.
        if (Vault::fileNameFor(write.title) != write.title)
            set(u"title"_s, write.title);
        if (note.created.isValid())
            set(u"created"_s, iso(note.created));
        QStringList tags;
        for (const QString &tagId : from.noteTags.value(note.id)) {
            if (tagTitles.contains(tagId))
                tags.append(tagTitles[tagId]);
        }
        if (!tags.isEmpty())
            set(u"tags"_s, tags);
        if (note.isTodo) {
            set(u"task"_s, note.completed.isValid() ? u"done"_s : u"open"_s);
            if (note.due.isValid())
                set(u"due"_s, iso(note.due));
            if (note.completed.isValid())
                set(u"done"_s, iso(note.completed));
        }
        if (!note.sourceUrl.isEmpty())
            set(u"source_url"_s, note.sourceUrl);
        if (!note.author.isEmpty())
            set(u"author"_s, note.author);

        if (!dryRun) {
            QString why;
            if (!notefile::write(vault.absolute(write.path), text, &why))
                return fail(QObject::tr("could not write %1: %2").arg(write.path, why));
            if (note.updated.isValid()) {
                QFile file(vault.absolute(write.path));
                if (file.open(QIODevice::ReadWrite))
                    file.setFileTime(note.updated, QFileDevice::FileModificationTime);
            }
        }
        ledger.notes[note.id] = {write.path, iso(note.updated), sha(text)};
        // Now and then on the way, so a long import cut short leaves little to bring again.
        if (++written % 50 == 0) {
            QString why;
            if (!saveLedger(&why))
                return fail(QObject::tr("could not write %1: %2").arg(kLedger, why));
        }
    }

    if (trashed > 0)
        result.skipped.append(trashed == 1
                                  ? QObject::tr("1 note in Joplin's trash")
                                  : QObject::tr("%1 notes in Joplin's trash").arg(trashed));
    if (missingFiles > 0)
        result.skipped.append(
            missingFiles == 1
                ? QObject::tr("1 attachment the export left out")
                : QObject::tr("%1 attachments the export left out").arg(missingFiles));

    if (unwritten > 0)
        result.skipped.append(
            unwritten == 1
                ? QObject::tr("1 attachment that could not be written, so its link stays as it was")
                : QObject::tr(
                      "%1 attachments that could not be written, so their links stay as they were")
                      .arg(unwritten));

    QString why;
    if (!saveLedger(&why)) {
        *error = QObject::tr("could not write %1: %2").arg(kLedger, why);
        return std::nullopt;
    }
    // A notebook keeps its emoji, unless one was chosen for it in Jot.
    if (!dryRun && !emojiOf.isEmpty()) {
        std::optional<Layout> read = Layout::read(vault, &why);
        if (!read) {
            *error = why;
            return std::nullopt;
        }
        Layout &layout = *read;
        for (auto it = emojiOf.cbegin(); it != emojiOf.cend(); ++it) {
            if (layout.emoji(it.key()).isEmpty())
                layout.setEmoji(it.key(), it.value());
        }
        if (!layout.write(vault, &why)) {
            *error = QObject::tr("could not write %1: %2").arg(Layout::kPath, why);
            return std::nullopt;
        }
    }
    return result;
}

} // namespace jot
