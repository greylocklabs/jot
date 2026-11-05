#pragma once

#include <QString>

namespace jot {

/// Where a data file stops making sense as its format, and why.
struct DataProblem
{
    /// From 1; 0 when the file reads.
    int line = 0;
    /// From 1, in characters, an emoji counting once.
    int column = 0;
    QString message;
    /// Where it is in the text, in UTF-16 units as QString counts them.
    qsizetype position = 0;

    [[nodiscard]] bool ok() const { return line == 0; }
};

/// Checks `text` as `kind`: "json", "yaml", "csv" or "tsv". Any other kind reads.
[[nodiscard]] DataProblem checkData(const QString &kind, const QString &text);

/// A problem at UTF-16 position `index` in `text`, with its line and column worked out.
[[nodiscard]] DataProblem problemAt(const QString &text, qsizetype index, const QString &message);

/// The kind of data file `path` names, by its extension, with `yml` as "yaml"; empty for
/// anything else.
[[nodiscard]] QString dataKindOf(const QString &path);

} // namespace jot
