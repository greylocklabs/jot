#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace jot {

/// A folder or a note in a vault, by its path from the vault's root with `/` between folders.
struct VaultEntry
{
    QString path;
    bool folder = false;

    bool operator==(const VaultEntry &) const = default;
};

/// A folder of Markdown notes, as docs/FORMAT.md describes it. Folders are notebooks and
/// `.md` files are notes. Nothing here is cached: the files are the truth.
class Vault
{
public:
    /// The vault format this build reads and writes.
    static constexpr int kFormat = 1;

    explicit Vault(QString root);

    /// Makes `root` a vault, creating the folder if needed and recording the format in
    /// `.jot/vault.yaml`. An existing vault is left as it is.
    static std::optional<Vault> create(const QString &root, QString *error);

    [[nodiscard]] QString root() const { return m_root; }
    [[nodiscard]] bool exists() const;
    /// The format the vault says it has, 1 when it says nothing.
    [[nodiscard]] int format() const;
    /// False for a vault from a newer Jot, which this one only reads.
    [[nodiscard]] bool writable() const { return format() <= kFormat; }

    /// Every folder and note, parents before children, each folder's contents in
    /// natural order ("Ch 2" before "Ch 10"). Hidden files, `.jot` and each note's
    /// `.assets` folder are left out.
    [[nodiscard]] QList<VaultEntry> entries() const;
    /// The notes alone, in the same order.
    [[nodiscard]] QStringList notes() const;
    [[nodiscard]] QString absolute(const QString &path) const;
    /// The vault path of a file inside the vault, or empty for one outside it.
    [[nodiscard]] QString relative(const QString &absolutePath) const;

    /// Creates a note called `title` in `folder`, with `created` in its front matter and
    /// `title` too when the file name cannot hold it. Gives its vault path.
    std::optional<QString> createNote(const QString &folder, const QString &title,
                                      const QDateTime &created, QString *error) const;
    /// Moves a note and its `.assets` folder to the system trash, or to the vault's own
    /// `.jot/trash` where the drive has none.
    bool trashNote(const QString &path, QString *error) const;
    /// Moves a notebook, with every note and attachment in it, to the trash as trashNote()
    /// does a note. Not the vault itself, nor a hidden folder such as `.jot`.
    bool trashFolder(const QString &path, QString *error) const;
    /// Uses the vault's own trash even where the desktop has one, as tests do.
    void setSystemTrash(bool use) { m_systemTrash = use; }
    /// Renames or moves a note, its `.assets` folder and its history. Links to it are the
    /// index's job.
    bool moveNote(const QString &from, const QString &to, QString *error) const;

    /// Makes a notebook called `name` in the notebook `parent`, or "name 2" and so on when
    /// that is taken. Gives its vault path.
    std::optional<QString> createFolder(const QString &parent, const QString &name,
                                        QString *error) const;
    /// Renames or moves a notebook with everything in it. Links are the caller's job.
    bool moveFolder(const QString &from, const QString &to, QString *error) const;

    /// Keeps `bytes` as an attachment of the note at `notePath`, in its `.assets` folder,
    /// as `fileName` or, when that is taken, "name 2.ext". Gives the attachment's vault path.
    std::optional<QString> addAttachment(const QString &notePath, const QString &fileName,
                                         const QByteArray &bytes, QString *error) const;
    /// As addAttachment(), copying the file at `source` rather than holding it in memory.
    std::optional<QString> copyAttachment(const QString &notePath, const QString &source,
                                          QString *error) const;
    /// A Markdown link target for `to` written in the note at `from`: relative to the note's
    /// folder, with spaces as %20, and in angle brackets when it holds a parenthesis.
    [[nodiscard]] static QString linkTarget(const QString &from, const QString &to);

    /// A path for a new note called `title` in `folder` that nothing has yet:
    /// "Pancakes.md", then "Pancakes 2.md".
    [[nodiscard]] QString freePath(const QString &folder, const QString &title) const;

    /// The title a note's file name gives it.
    [[nodiscard]] static QString titleFromPath(const QString &path);
    /// What a wikilink to the note at `path` says: its file name, or its path without `.md`
    /// when another note shares the name, since wikilinks find notes by file name.
    [[nodiscard]] static QString wikilinkName(const QString &path, bool shared);
    /// The notebook a note is in, by vault path; empty for the top of the vault.
    [[nodiscard]] static QString notebookOf(const QString &path);
    /// `title` as a file name without `.md`: characters some sync targets refuse are
    /// replaced, and leading and trailing dots and spaces trimmed.
    [[nodiscard]] static QString fileNameFor(const QString &title);
    /// The folder beside a note that holds its attachments.
    [[nodiscard]] static QString assetsFolderFor(const QString &notePath);
    /// The folder under `.jot/history` that holds the past versions of a note, or of every
    /// note in a notebook.
    [[nodiscard]] static QString historyFolderFor(const QString &path);

private:
    void collect(const QString &folder, QList<VaultEntry> &into) const;

    QString m_root;
    bool m_systemTrash = true;

    /// Moves the file or folder at vault path `relative` to the desktop's trash, or to the
    /// vault's own `.jot/trash/<stamp>` where the desktop has none.
    bool moveToTrash(const QString &relative, const QString &stamp) const;
    /// Moves the history of the note or notebook at `from` to `to`.
    bool moveHistory(const QString &from, const QString &to) const;
};

/// Orders names as people read them: case aside, and "Ch 2" before "Ch 10".
[[nodiscard]] int naturalCompare(QStringView a, QStringView b);

/// Reading and writing note files without losing a byte.
namespace notefile {

/// The note's text, exactly as stored. A file that is not valid UTF-8 is refused rather
/// than read with replacement characters that a save would then write back.
[[nodiscard]] std::optional<QString> read(const QString &path, QString *error);

/// Writes `text` as UTF-8 through a temporary file and a rename, so a crash leaves the
/// old note or the new one, never half of each. A symbolic link stays a link, and a file
/// with other hard links is written in place so they keep seeing it.
bool write(const QString &path, const QString &text, QString *error);

} // namespace notefile

} // namespace jot
