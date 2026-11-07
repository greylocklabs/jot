#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

#include <functional>
#include <optional>

namespace jot {

class Vault;

/// A note or data file from a vault, in its trash.
struct TrashedNote
{
    /// Where it is now, as an absolute path.
    QString location;
    /// The vault path it had. Only its name where the trash kept no folder, as Jot 0.3's did.
    QString path;
    QDateTime deleted;
    /// Its `.assets` folder in the same trash, as an absolute path; empty when that did not go.
    QString assets;
};

/// What the vault's own trash, `.jot/trash`, holds: listing it, bringing notes back, and deleting
/// them for good or handing them on to the desktop's trash.
class VaultTrash
{
public:
    /// Moves a file or folder to the desktop's trash; QFile::moveToTrash, but for tests.
    using Discard = std::function<bool(const QString &)>;

    explicit VaultTrash(const Vault &vault);

    /// Every note and data file in the trash, newest first. One in a trashed notebook is listed
    /// on its own.
    [[nodiscard]] QList<TrashedNote> list() const;

    /// Puts `note` back at its path, or a free one beside it when that is taken now, with its
    /// attachments. Gives the vault path it went to. Attachments that cannot follow stay in the
    /// trash, and `error` says so though the note is back.
    std::optional<QString> restore(const TrashedNote &note, QString *error) const;

    /// Deletes `note` and its attachments for good.
    bool remove(const TrashedNote &note, QString *error) const;
    /// Deletes everything in the trash for good.
    bool empty(QString *error) const;
    /// Hands what was deleted more than `days` days before `now` to the desktop's trash, each
    /// note by its own name. What the desktop will not take stays. Gives how many notes went.
    int expire(const QDateTime &now, int days, const Discard &discard) const;

private:
    /// The trash's folder, absolute.
    [[nodiscard]] QString folder() const;
    /// Takes away the folders `from` leaves empty, up to the trash itself.
    void prune(const QString &from) const;

    const Vault &m_vault;
};

} // namespace jot
