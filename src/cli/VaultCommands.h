#pragma once

#include "Paint.h"

#include "jot/Vault.h"
#include "jot/VaultIndex.h"

#include <QCoreApplication>
#include <QTextStream>

#include <functional>

namespace jot::cli {

/// The commands that work on a vault: status, ls, search, new, show, tasks, tags and import. Each
/// writes what it found to `out` and problems to `err`, and gives an exit code: 0 for
/// success, 1 for a failure, 2 for a mistake in how it was asked.
class VaultCommands
{
    Q_DECLARE_TR_FUNCTIONS(VaultCommands)

public:
    VaultCommands(const Vault &vault, VaultIndex &index, QTextStream &out, QTextStream &err);

    /// Colors for a terminal; output is plain without them.
    void setPaint(const Paint &paint) { m_paint = paint; }

    int status(const QString &indexPath, bool json);
    /// Notes in `notebook` and those inside it, every note when it is empty.
    int list(const QString &notebook, const QString &order, bool json);
    int search(const QString &query, bool json);
    /// Makes a note called `title` in `notebook`, with `body` after its front matter.
    int create(const QString &title, const QString &notebook, const QString &body, bool json);
    /// Prints the note `name` names: a path, a title or an alias.
    int show(const QString &name);
    /// Adds a to-do, read from `typed` as quick add reads it, to the inbox note.
    int addTask(const QString &typed, bool json);
    /// The to-dos left in `view`, "all" when empty, with `label` and `tag` when given.
    int listTasks(bool json, const QString &view = {}, const QString &label = {},
                  const QString &tag = {});
    /// Every tag, with how many notes have it or a tag nested in it.
    int listTags(bool json);
    /// Imports the Joplin JEX export at `file` under `notebook`, or says what it would do.
    int importJoplin(const QString &file, const QString &notebook, bool dryRun, bool json);
    /// Starts `program`, as the app, with these arguments; says whether it started.
    using Launcher = std::function<bool(const QString &program, const QStringList &arguments)>;
    /// Opens the note `name` names in the app at `program`, through `launch`.
    int open(const QString &name, const QString &program, const Launcher &launch);

    /// The note `name` names, by path, title or alias, or empty.
    [[nodiscard]] QString find(const QString &name) const;

private:
    const Vault &m_vault;
    VaultIndex &m_index;
    QTextStream &m_out;
    QTextStream &m_err;
    Paint m_paint;
};

} // namespace jot::cli
