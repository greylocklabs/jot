#pragma once

#include "jot/Vault.h"

#include <QDate>
#include <QHash>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

#include <optional>

namespace jot {

/// What a note's front matter and file name say about it.
struct NoteInfo
{
    QString path;
    QString title;
    QStringList tags;
    QStringList aliases;
    /// "open", "done" or empty for a note that is not a task.
    QString task;
    QString due;
    QString created;
    QString type;
    /// When the file last changed, in nanoseconds since 1970.
    qint64 modified = 0;
};

/// A link in a note: to another note, an attachment, or something not there.
struct NoteLink
{
    QString source;
    /// As written: "Mara", "Cast/Mara.md#Voice" or "Mara.assets/sketch.png".
    QString raw;
    /// The vault path it reaches, or empty when it reaches nothing.
    QString target;
    /// "link" for [x](y), "wiki" for [[y]], "embed" for ![x](y) or ![[y]].
    QString kind;
    int line = 0;

    bool operator==(const NoteLink &) const = default;
};

/// A checkbox in a note, as `- [ ] text`.
struct NoteTask
{
    QString path;
    int line = 0;
    bool done = false;
    QString text;

    bool operator==(const NoteTask &) const = default;
};

struct SearchHit
{
    QString path;
    QString title;
    /// The matching text, with each match between <mark> and </mark>.
    QString snippet;
    qint64 modified = 0;
};

/// Whether a task due on `due`, as front matter writes it (a date, or a date and time), is
/// past due on `today`. A due that is not a date never is.
[[nodiscard]] bool isOverdue(const QString &due, const QDate &today);

/// Puts `notes` in `order`, one of Settings::noteSorts(): "updated" most recently changed first,
/// "created" most recently made first by the `created` front matter key (or when it last
/// changed, for a note without one), "title" by title. Notes in `pinned` come first, in the
/// same order among themselves.
void sortNotes(QList<NoteInfo> &notes, const QString &order, const QStringList &pinned = {});

/// What Jot knows about a vault's notes, kept in SQLite for search, links, tags and
/// tasks. It is a cache: deleting it loses nothing, and refresh() rebuilds it from the
/// files. It lives outside the vault, so syncing a vault never syncs its index.
class VaultIndex
{
public:
    /// Raised whenever what the index stores changes shape; an older index is rebuilt.
    static constexpr int kSchema = 3;

    explicit VaultIndex(Vault vault, QString databasePath = {});
    ~VaultIndex();
    VaultIndex(const VaultIndex &) = delete;
    VaultIndex &operator=(const VaultIndex &) = delete;

    /// $XDG_CACHE_HOME/jot/index/<hash of the vault's path>.sqlite
    [[nodiscard]] static QString defaultPath(const Vault &vault);

    bool open(QString *error);
    /// Compares every note with what the index holds and reads those that changed.
    /// Gives the paths it read or dropped.
    QStringList refresh();
    /// Reads these notes again, or drops those that are gone, as the watcher reports them.
    void update(const QStringList &paths);

    [[nodiscard]] std::optional<NoteInfo> note(const QString &path) const;
    [[nodiscard]] int noteCount() const;
    /// The notes in `folder` and the folders inside it; every note for an empty folder.
    [[nodiscard]] QList<NoteInfo> notesIn(const QString &folder) const;
    /// How many notes each folder holds, counting those in its own folders, by folder path.
    [[nodiscard]] QHash<QString, int> noteCounts() const;

    /// Notes matching `query`: words, each matched at the start of a word, plus any of
    /// `tag:name` (with its nested tags), `in:folder`, `task:open`, `task:done` and
    /// `type:name`. Best matches first, titles counting most.
    [[nodiscard]] QList<SearchHit> search(const QString &query, int limit = 50) const;

    [[nodiscard]] QList<NoteLink> linksFrom(const QString &path) const;
    /// Links that reach `path`, from other notes.
    [[nodiscard]] QList<NoteLink> backlinks(const QString &path) const;
    /// The notes with a link to anything inside `folder`, a note or an attachment.
    [[nodiscard]] QStringList sourcesLinkingInto(const QString &folder) const;
    /// Links that reach nothing, so a note could be made for them.
    [[nodiscard]] QList<NoteLink> unresolved() const;
    /// Where a link written in `from` leads, the way Obsidian reads it: a path from the
    /// note's folder or the vault's root, then the shortest unique name, then an alias.
    [[nodiscard]] QString resolve(const QString &raw, const QString &from) const;

    /// Every tag in use, nested ones as written, sorted.
    [[nodiscard]] QStringList tags() const;
    /// Notes with `tag` or a tag nested in it.
    [[nodiscard]] QStringList tagged(const QString &tag) const;
    /// Unticked checkboxes across the vault, note by note.
    [[nodiscard]] QList<NoteTask> openTasks() const;
    /// Notes whose front matter makes them tasks that are still open, soonest due first.
    [[nodiscard]] QList<NoteInfo> openTaskNotes() const;

private:
    bool createSchema(QString *error);
    void read(const QString &path);
    void drop(const QString &path);
    /// Links are resolved against the whole vault, so any note added, renamed or
    /// removed can change where others lead.
    void resolveLinks();
    [[nodiscard]] NoteInfo infoFrom(const QSqlQuery &query) const;

    Vault m_vault;
    QString m_path;
    QString m_connection;
};

} // namespace jot
