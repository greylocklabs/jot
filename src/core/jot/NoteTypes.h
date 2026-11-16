#pragma once

#include "jot/FrontMatter.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <functional>
#include <optional>

namespace jot {

/// A key a type gives its notes, and what kind of value it holds.
struct TypeProperty
{
    enum class Kind { Text, Number, Date, Checkbox, Url, Choice, Link, Image };

    QString key;
    Kind kind = Kind::Text;
    /// Holds a list of values of its kind rather than one.
    bool list = false;
    /// For a choice, the values it can take.
    QStringList choices;
    /// For a link, the type the note it leads to has, or empty for any note.
    QString linkType;

    bool operator==(const TypeProperty &) const = default;
};

/// What `kind` is called in a type file: "text", "number" and so on.
[[nodiscard]] QString kindName(TypeProperty::Kind kind);

/// A kind of note, as a file in `.jot/types` describes it: what its notes are called and look
/// like, and the keys they have. docs/FORMAT.md describes the file.
struct NoteType
{
    /// The file's name without `.yaml`.
    QString id;
    /// What notes say in `type:`, the file's `name:` or else its id.
    QString name;
    /// A Glyph name, or empty.
    QString icon;
    /// One of colorNames(), a `#rrggbb` color, or empty.
    QString color;
    /// The type it builds on, as named, or empty.
    QString extends;
    /// The note a new note of this type is made from, as a path from the vault's root.
    QString templatePath;
    /// Its own properties and those of the types it extends, theirs first.
    QList<TypeProperty> properties;

    /// The property with `key`, or none.
    [[nodiscard]] const TypeProperty *property(const QString &key) const;
};

/// Something wrong in a type file, on its line, counted from 1.
struct TypeFileProblem
{
    int line = 0;
    QString message;
    bool operator==(const TypeFileProblem &) const = default;
};

/// The vault's note types, read from `.jot/types`.
class NoteTypes
{
public:
    /// Where a vault keeps its types, from the vault's root.
    static inline const QString kFolder = QStringLiteral(".jot/types");

    /// Reads every type in the vault at `root`. A file that cannot be read is left out.
    [[nodiscard]] static NoteTypes read(const QString &root);
    /// Reads `files`, each a type's id and its file's text, as one vault's types.
    [[nodiscard]] static NoteTypes fromFiles(const QHash<QString, QString> &files);

    /// Every type, by name.
    [[nodiscard]] QList<NoteType> all() const { return m_types; }
    /// The type a note's `type:` names, by its name or id in any case, or none.
    [[nodiscard]] const NoteType *find(const QString &name) const;
    /// Whether `name` is the type `base` or one that extends it, at any depth.
    [[nodiscard]] bool isA(const QString &name, const QString &base) const;
    /// What is wrong in each type's file, by id.
    [[nodiscard]] QList<TypeFileProblem> problems(const QString &id) const
    {
        return m_problems.value(id);
    }

    /// The types every vault starts with, by id.
    [[nodiscard]] static QStringList starterIds();
    /// The text of the starter type `id`.
    [[nodiscard]] static QString starterText(const QString &id);
    /// Writes the starter types into the vault at `root` when it has no types folder yet, so a
    /// vault whose types were all deleted keeps none.
    static bool writeStartersIfMissing(const QString &root, QString *error);
    /// The starter types restoreStarters() would write, by id: those missing or changed.
    [[nodiscard]] static QStringList startersToRestore(const QString &root);
    /// Writes each starter type that is missing or differs from Jot's own, leaving other types
    /// as they are.
    static bool restoreStarters(const QString &root, QString *error);

private:
    QList<NoteType> m_types;
    QHash<QString, QList<TypeFileProblem>> m_problems;
};

/// The named colors a type can take, which the theme gives its own shade.
[[nodiscard]] QStringList colorNames();

/// Whether `path`, from the vault's root, is a type's file.
[[nodiscard]] bool isNoteTypeFile(const QString &path);

/// What is wrong with the note's type: a `type:` naming none of `types`, or a key with a value
/// its type does not take, each on the key's line. `typeOfLink` gives the type of the note a
/// link value leads to, empty when it has none, or nothing when it leads to no note.
[[nodiscard]] QList<FrontMatterProblem>
noteTypeProblems(QStringView note, const NoteTypes &types,
                 const std::function<std::optional<QString>(const QString &)> &typeOfLink);

/// A key of a note's front matter as its properties show it.
struct PropertyRow
{
    QString key;
    /// What the note's type says of the key, or nothing for one it does not name.
    std::optional<TypeProperty> property;
    /// As readFrontMatter() gives it, or invalid when the note leaves the key out.
    QVariant value;
};

/// A note's properties in the order to show them: `type`, then its type's properties in the
/// type's order, those the note leaves out too, then the note's other keys as written.
[[nodiscard]] QList<PropertyRow> propertyRows(QStringView note, const NoteTypes &types);

/// `value` as a link property writes it, `[[Ash Court]]` for `Ash Court`.
[[nodiscard]] QString linkValue(const QString &value);

/// The note a link value leads to as written, `Ash Court` for `[[Ash Court|the Court]]` or
/// `Ash Court`, or empty.
[[nodiscard]] QString linkValueTarget(const QString &value);

} // namespace jot
