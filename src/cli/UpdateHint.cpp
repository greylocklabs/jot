#include "UpdateHint.h"

#include <QObject>
#include <QStringList>

using namespace Qt::StringLiterals;

namespace jot::cli {

UpdateHint UpdateHint::after(const QString &command, bool json, bool terminal, bool setting,
                             const greylock::UpdateCheck &check, const QVersionNumber &running)
{
    // Commands about releases or for other programs say nothing of their own.
    static const QStringList quiet = {u"upgrade"_s,    u"downgrade"_s,   u"update-check"_s,
                                      u"__complete"_s, u"completions"_s, u"gui"_s};
    UpdateHint hint;
    if (json || !terminal || quiet.contains(command) || !check.wanted(setting))
        return hint;
    if (const std::optional<greylock::Release> newer = check.newerThan(running)) {
        hint.line = QObject::tr("jot: Jot %1 is out, and this is %2. Update with: "
                                "jot upgrade")
                        .arg(newer->version.toString(), running.toString());
    }
    hint.ask = check.due();
    return hint;
}

} // namespace jot::cli
