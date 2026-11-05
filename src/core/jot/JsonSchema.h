#pragma once

#include "jot/DataCheck.h"

#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace jot {

/// A value that breaks a schema: where it is, as the keys and indexes leading to it from the
/// top ("3", "name"), and why.
struct SchemaProblem
{
    QStringList path;
    QString message;

    bool operator==(const SchemaProblem &) const = default;
};

/// Checks `data` against `schema`, JSON Schema draft 2020-12: types, enum and const, numbers'
/// and strings' bounds and patterns, arrays' items, objects' properties and required keys,
/// the allOf, anyOf, oneOf and not combinations, if/then/else, and $ref within the schema.
/// `format` is not checked, as the draft leaves it to annotate.
[[nodiscard]] QList<SchemaProblem> checkSchema(const QJsonValue &schema, const QJsonValue &data);

/// A data file's text as JSON values, to check: JSON as it is, YAML through its core schema,
/// and CSV or TSV as one object per row, a number or true or false read as one and empty
/// fields left out.
[[nodiscard]] std::optional<QJsonValue> dataAsJson(const QString &kind, const QString &text,
                                                   QString *error = nullptr);

/// The schema file for the data file at `path`: what `$schema` at its top names, when that is
/// a file beside it, else `<name>.schema.json` beside it. Empty when it has none. Only files on
/// disk are read; a schema on the web is never fetched.
[[nodiscard]] QString schemaPathFor(const QString &path, const QJsonValue &data);

/// Everything wrong with the data file at `path` holding `text`: where it stops reading, or
/// what in it breaks its schema, each at its line and column.
[[nodiscard]] QList<DataProblem> checkDataFile(const QString &path, const QString &text);

} // namespace jot
