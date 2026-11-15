#pragma once

#include <QString>

namespace jot {

/// What a bug report needs: versions, environment, and the end of each log.
/// Email addresses are masked throughout.
namespace diagnostics {

/// Replaces each distinct email address with <email-1>, <email-2> and so on,
/// and the home folder with ~.
[[nodiscard]] QString redact(const QString &text);

/// Builds the report. `extra` adds lines such as the GUI's platform plugin.
[[nodiscard]] QString collect(const QString &extra = {});

} // namespace diagnostics
} // namespace jot
