#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>

namespace jot {

class Vault;

/// The notebook whose notes are templates: `templates:` in `.jot/vault.yaml`, or `Templates`.
[[nodiscard]] QString templatesFolder(const Vault &vault);

/// The notebook daily notes go in: `daily:` in `.jot/vault.yaml`, or `Daily`.
[[nodiscard]] QString dailyFolder(const Vault &vault);

/// The daily note for `day`, as `<daily folder>/YYYY-MM-DD.md`.
[[nodiscard]] QString dailyNotePath(const Vault &vault, QDate day);

/// The template daily notes are made from, `Daily.md` in the templates notebook.
[[nodiscard]] QString dailyTemplatePath(const Vault &vault);

/// `text` with its variables filled in: `{{title}}`, `{{date}}` as YYYY-MM-DD, `{{time}}` as
/// HH:mm, and `{{date:FORMAT}}` or `{{time:FORMAT}}` in a format as Obsidian writes them, such
/// as `dddd D MMMM YYYY`, with text to keep in square brackets. Any other `{{...}}` stays as
/// written.
[[nodiscard]] QString fillTemplate(const QString &text, const QString &title, const QDateTime &now);

/// A note made from `filled`, a filled template, on top of `made`, the text a new note starts
/// with: the template's front matter keys join the note's, and its body follows.
[[nodiscard]] QString noteFromTemplate(const QString &made, const QString &filled);

} // namespace jot
