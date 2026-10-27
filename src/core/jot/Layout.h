#pragma once

#include "jot/Vault.h"

#include <QHash>
#include <QMap>
#include <QString>
#include <QStringList>

#include <optional>

namespace jot {

/// How the vault's notes and notebooks are shown, as `.jot/layout.yaml` keeps it beside them,
/// so it syncs with the notes: each folder's manual order, and each notebook's emoji, icon
/// and color. docs/FORMAT.md describes the file.
class Layout
{
public:
    /// Where the vault keeps it.
    static inline const QString kPath = QStringLiteral(".jot/layout.yaml");

    /// What the vault's layout file says, empty when there is none. Nothing, with `error` set,
    /// when the file is there but cannot be read, so it is not written over.
    [[nodiscard]] static std::optional<Layout> read(const Vault &vault, QString *error = nullptr);
    /// Writes it to the vault, leaving out entries for notebooks and notes that are gone. What
    /// Jot does not use, such as other sections or nested values under a notebook, is written
    /// back as it was; comments inside the sections Jot writes are not. A vault without the file
    /// only gets one once there is something to keep.
    bool write(const Vault &vault, QString *error) const;
    /// After a note or notebook moved from `from` to `to`, what the layout says about it, and
    /// about everything inside a notebook, goes with it. Nothing is written when it says nothing,
    /// nor when the file cannot be read.
    static bool follow(const Vault &vault, const QString &from, const QString &to, QString *error);

    /// The emoji shown before the notebook's name, or empty.
    [[nodiscard]] QString emoji(const QString &folder) const;
    /// Gives the notebook an emoji, or takes it away when `emoji` is empty.
    void setEmoji(const QString &folder, const QString &emoji);
    /// Every notebook's emoji, by its path.
    [[nodiscard]] QHash<QString, QString> emojis() const;

    /// The same move as follow(), on this layout alone.
    void move(const QString &from, const QString &to);
    /// The file's text.
    [[nodiscard]] QString text() const;

private:
    /// Each folder's notes and folders in the order given, by name.
    QMap<QString, QStringList> m_order;
    /// Each folder's appearance: emoji, icon, color, and any other key, by folder path.
    QMap<QString, QMap<QString, QString>> m_folders;
    /// Lines under a folder that are not `key: value`, such as a nested map, kept as written
    /// but indented from the folder's keys.
    QMap<QString, QStringList> m_folderLines;
    /// Comments before the first section, kept at the top.
    QStringList m_header;
    /// Sections Jot does not know, kept as written.
    QStringList m_other;
};

} // namespace jot
