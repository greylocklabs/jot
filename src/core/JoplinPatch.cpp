#include "jot/JoplinPatch.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace jot {

std::optional<QString> applyJoplinPatch(const QString &text, const QString &patch)
{
    if (patch.isEmpty())
        return text;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(patch.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isArray())
        return std::nullopt;

    QString out = text;
    // How far the text has drifted from where the patches expect it, by earlier patches that
    // went somewhere other than where they said.
    qsizetype drift = 0;
    for (const QJsonValue &value : document.array()) {
        const QJsonObject each = value.toObject();
        if (!each.value(u"diffs"_s).isArray() || !each.value(u"start2"_s).isDouble())
            return std::nullopt;
        QString before;
        QString after;
        for (const QJsonValue &diff : each.value(u"diffs"_s).toArray()) {
            const QJsonArray pair = diff.toArray();
            const int op = pair.at(0).toInt(2);
            const QString part = pair.at(1).toString();
            if (op < -1 || op > 1)
                return std::nullopt;
            if (op <= 0)
                before += part;
            if (op >= 0)
                after += part;
        }
        const qsizetype expected = qsizetype(each.value(u"start2"_s).toDouble()) + drift;
        qsizetype at = -1;
        if (expected >= 0 && QStringView(out).mid(expected, before.size()) == before) {
            at = expected;
        } else {
            const qsizetype later = out.indexOf(before, std::max<qsizetype>(expected, 0));
            const qsizetype earlier =
                expected > 0 ? out.lastIndexOf(before, std::min(expected, out.size())) : -1;
            if (later < 0 || (earlier >= 0 && expected - earlier < later - expected))
                at = earlier;
            else
                at = later;
        }
        if (at < 0)
            return std::nullopt;
        drift += at - expected;
        out.replace(at, before.size(), after);
    }
    return out;
}

} // namespace jot
