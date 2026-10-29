#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

namespace jot {

class Vault;

/// A note's past versions, kept as plain copies in the vault under `.jot/history/<note path>/`
/// and named by when the note held each, such as `2026-10-28 153012.md`.
class NoteHistory
{
public:
    struct Version
    {
        /// When the note held this text.
        QDateTime written;
        /// The copy's vault path.
        QString path;
    };

    explicit NoteHistory(const Vault &vault) : m_vault(vault) {}

    /// The versions of `note`, newest first.
    [[nodiscard]] QList<Version> versions(const QString &note) const;
    /// Keeps `text` as the version of `note` written at `written`. An empty text, or one the
    /// same as the newest version, is not kept again.
    bool keep(const QString &note, const QString &text, const QDateTime &written,
              QString *error) const;
    /// Keeps `text` as the version of `note` written at `written`, whatever versions it has.
    bool add(const QString &note, const QString &text, const QDateTime &written,
             QString *error) const;

private:
    const Vault &m_vault;
};

} // namespace jot
