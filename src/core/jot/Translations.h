#pragma once

class QCoreApplication;

namespace jot {

/// Puts the app's words in place: the desktop's language where Jot has it, else English as
/// translations/jot_en.ts words it, which falls back to the words in the code.
void installTranslations(QCoreApplication &app);

} // namespace jot
