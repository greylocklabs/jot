#pragma once

#include <QString>
#include <QUrl>

#include <functional>

namespace jot {

/// What a bug report needs: versions, environment, and the end of each log.
/// Email addresses are masked throughout.
namespace diagnostics {

/// Replaces each distinct email address with <email-1>, <email-2> and so on,
/// and the home folder with ~.
[[nodiscard]] QString redact(const QString &text);

/// Builds the report. `extra` adds lines such as the GUI's platform plugin.
[[nodiscard]] QString collect(const QString &extra = {});

/// A new GitHub issue with `report` filled in, shortened to fit in a URL.
[[nodiscard]] QUrl issueUrl(const QString &report);

/// The longest new-issue address GitHub accepts.
inline constexpr qsizetype kMaxIssueUrlLength = 8000;

/// What issueUrl() ends a shortened report with: where to find the rest.
[[nodiscard]] QString shortenedMark();

/// The address `build` makes from `text`, with `text` cut from its end and
/// `mark` added until the address fits in a URL GitHub accepts.
[[nodiscard]] QUrl fitIssueUrl(const std::function<QUrl(const QString &)> &build,
                               const QString &text, const QString &mark);

} // namespace diagnostics
} // namespace jot
