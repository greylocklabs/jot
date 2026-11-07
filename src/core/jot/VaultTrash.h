#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace jot {

class Vault;

/// A note or data file from a vault, in a trash.
struct TrashedNote
{
    /// Where it is now, as an absolute path.
    QString location;
    /// The vault path it had. Only its name where the trash kept no folder, as Jot 0.3's own did.
    QString path;
    QDateTime deleted;
    /// Its `.assets` folder in the same trash, as an absolute path; empty when that did not go.
    QString assets;
    /// The desktop trash's records of it and of its attachments, each removed once that is back.
    QString record;
    QString assetsRecord;
};

/// What the desktop's trash and the vault's own `.jot/trash` hold from a vault, and bringing it
/// back. Jot never empties either; that stays with the desktop.
class VaultTrash
{
public:
    /// `desktopTrash` is the desktop's trash folder, holding `files` and `info`.
    explicit VaultTrash(const Vault &vault, QString desktopTrash = homeTrash());

    /// `$XDG_DATA_HOME/Trash`, where the desktop puts what is deleted from the home drive.
    [[nodiscard]] static QString homeTrash();

    /// Every note and data file from the vault in either trash, newest first. One in a trashed
    /// notebook is listed on its own.
    [[nodiscard]] QList<TrashedNote> list() const;

    /// Puts `note` back at its path, or a free one beside it when that is taken now, with its
    /// attachments. Gives the vault path it went to. Attachments that cannot follow stay in the
    /// trash, as the desktop can still restore them, and `error` says so though the note is back.
    std::optional<QString> restore(const TrashedNote &note, QString *error) const;

private:
    void listDesktop(QList<TrashedNote> &into) const;
    void listOwn(QList<TrashedNote> &into) const;

    const Vault &m_vault;
    QString m_desktopTrash;
};

} // namespace jot
