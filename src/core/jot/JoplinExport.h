#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace jot {

/// What a Joplin JEX export or Joplin's database holds: Joplin's items, with the attachments'
/// files. Where each note was written is never read, as Jot does not keep it.
struct JoplinExport
{
    struct Notebook
    {
        QString id;
        /// Empty for a top-level notebook.
        QString parentId;
        QString title;
        /// The notebook's emoji, when it has one.
        QString emoji;
        bool deleted = false;
    };
    struct Note
    {
        QString id;
        QString notebookId;
        QString title;
        QString body;
        QDateTime created;
        QDateTime updated;
        bool isTodo = false;
        /// Invalid when the to-do has no due time.
        QDateTime due;
        /// Invalid until the to-do is done.
        QDateTime completed;
        /// Written in HTML rather than Markdown.
        bool html = false;
        QString sourceUrl;
        QString author;
        /// A copy Joplin kept when two edits clashed in sync.
        bool conflict = false;
        /// In Joplin's trash.
        bool deleted = false;
    };
    struct Tag
    {
        QString id;
        QString title;
    };
    struct Resource
    {
        QString id;
        QString title;
        QString mime;
        /// The name the file had when it was attached, which may be empty.
        QString fileName;
        QString extension;
        /// The file's bytes, from an export; null when it left the file out.
        QByteArray data;
        /// Where the file is, from Joplin's database, read only when it is copied in; empty
        /// when the file is not there.
        QString file;
    };

    QList<Notebook> notebooks;
    QList<Note> notes;
    QList<Tag> tags;
    /// The ids of each note's tags, by note id.
    QHash<QString, QStringList> noteTags;
    QList<Resource> resources;
    /// What could not be read, in words for the summary: encrypted items, unknown types.
    QStringList skipped;
};

/// The items in a JEX export at `path`, or nothing with `error` set when it is not one.
[[nodiscard]] std::optional<JoplinExport> readJoplinExport(const QString &path,
                                                           QString *error = nullptr);

/// The same, from the export's bytes.
[[nodiscard]] std::optional<JoplinExport> readJoplinExport(const QByteArray &jex,
                                                           QString *error = nullptr);

/// The items in Joplin's own database at `path`, `database.sqlite` in a Joplin profile, with
/// attachments from the `resources` folder beside it. The database is only read, and is best
/// read while Joplin is closed. Nothing, with `error` set, when it is not Joplin's.
[[nodiscard]] std::optional<JoplinExport> readJoplinDatabase(const QString &path,
                                                             QString *error = nullptr);

/// The items at `path`: a JEX export, Joplin's database, or a Joplin profile folder holding one.
[[nodiscard]] std::optional<JoplinExport> readJoplin(const QString &path, QString *error = nullptr);

/// The database of the Joplin desktop app on this computer, installed as a package or a Flatpak,
/// or empty when there is none.
[[nodiscard]] QString joplinProfileDatabase();

} // namespace jot
