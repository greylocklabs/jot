#include "jot/TextFind.h"

namespace jot {

namespace {

bool isWordCharacter(QChar c)
{
    return c.isLetterOrNumber() || c == u'_' || c.isMark();
}

} // namespace

QList<qsizetype> findAll(QStringView text, QStringView needle, bool matchCase, bool wholeWords)
{
    QList<qsizetype> found;
    if (needle.isEmpty())
        return found;
    const Qt::CaseSensitivity sensitivity = matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
    qsizetype from = 0;
    while (true) {
        const qsizetype at = text.indexOf(needle, from, sensitivity);
        if (at < 0)
            break;
        const qsizetype end = at + needle.size();
        const bool whole = (at == 0 || !isWordCharacter(text[at - 1])) &&
                           (end == text.size() || !isWordCharacter(text[end]));
        if (!wholeWords || whole) {
            found.append(at);
            from = end;
        } else {
            from = at + 1;
        }
    }
    return found;
}

} // namespace jot
