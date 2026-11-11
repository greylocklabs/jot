#pragma once

#include "jot/Vault.h"
#include "jot/VaultIgnore.h"

#include <QByteArray>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>

namespace jot {

/// Says which notes changed on disk, whoever changed them: another editor, a sync tool
/// or a script. Watches each folder for notes coming and going and each note for writes
/// in place, and compares a folder with what it held before rather than trusting single
/// events, since a save by rename replaces the very file being watched.
class VaultWatcher : public QObject
{
    Q_OBJECT

public:
    explicit VaultWatcher(Vault vault, QObject *parent = nullptr);

    /// Records what the vault holds and starts watching. Changes before this are not
    /// reported; the index compares the files itself when it opens.
    void start();
    /// Compares every folder with what it held, as after a missed event or on waking.
    void rescan();
    /// Jot is about to write `text` to `path`, so seeing exactly that there is no news.
    void expectWrite(const QString &path, const QString &text);

    /// How long to wait for a burst of events to settle, in ms.
    void setDelay(int ms) { m_settle.setInterval(ms); }

Q_SIGNALS:
    /// Notes added, changed or removed, by vault path, settled and without duplicates.
    void changed(const QStringList &notes);
    /// Folders added or removed, so the notebook tree should be read again.
    void foldersChanged();
    /// A `.gitignore` in the vault changed, so what the vault holds should be read again whole.
    void ignoreChanged();

private:
    struct Seen
    {
        qint64 size = -1;
        qint64 modified = -1;
        qint64 inode = -1;

        bool operator==(const Seen &) const = default;
    };

    /// Notes directly in `folder` and its folders, as they are now.
    void snapshot(const QString &folder, QHash<QString, Seen> &notes, QSet<QString> &folders) const;
    void compare(const QString &folder);
    void flush();
    void watchNote(const QString &path);
    /// Records the `.gitignore` in `folder`; whether it differs from what was recorded before.
    bool noteGitignore(const QString &folder);
    [[nodiscard]] bool expected(const QString &path);

    Vault m_vault;
    VaultIgnore m_ignore;
    QHash<QString, Seen> m_gitignores;
    QFileSystemWatcher m_watcher;
    QTimer m_settle;
    QHash<QString, Seen> m_notes;
    QSet<QString> m_folders;
    QSet<QString> m_watchedNotes;
    QSet<QString> m_dirty;
    QSet<QString> m_pending;
    bool m_foldersChanged = false;
    QHash<QString, QByteArray> m_expected;
};

} // namespace jot
