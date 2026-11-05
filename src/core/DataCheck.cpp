#include "jot/DataCheck.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QObject>

#include <yaml.h>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// The line and column, from 1, of character `index` in `text`.
DataProblem at(const QString &text, qsizetype index, const QString &message)
{
    index = std::clamp<qsizetype>(index, 0, text.size());
    const qsizetype lineStart = text.lastIndexOf(u'\n', index - 1) + 1;
    return {int(text.left(index).count(u'\n')) + 1, int(index - lineStart) + 1, message};
}

DataProblem checkJson(const QString &text)
{
    const QByteArray bytes = text.toUtf8();
    QJsonParseError error;
    QJsonDocument::fromJson(bytes, &error);
    if (error.error == QJsonParseError::NoError)
        return {};
    // Qt counts the offset in UTF-8 bytes.
    const qsizetype index = QString::fromUtf8(bytes.left(error.offset)).size();
    return at(text, index, error.errorString());
}

DataProblem checkYaml(const QString &text)
{
    const QByteArray bytes = text.toUtf8();
    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser))
        return {};
    yaml_parser_set_input_string(&parser, reinterpret_cast<const unsigned char *>(bytes.data()),
                                 size_t(bytes.size()));
    DataProblem problem;
    for (;;) {
        yaml_event_t event;
        if (!yaml_parser_parse(&parser, &event)) {
            QString message = QString::fromUtf8(parser.problem ? parser.problem : "");
            if (parser.context)
                message = QString::fromUtf8(parser.context) + u", "_s + message;
            // libyaml counts lines and columns from 0, in characters.
            problem = {int(parser.problem_mark.line) + 1, int(parser.problem_mark.column) + 1,
                       message.isEmpty() ? QObject::tr("not valid YAML") : message};
            break;
        }
        const bool end = event.type == YAML_STREAM_END_EVENT;
        yaml_event_delete(&event);
        if (end)
            break;
    }
    yaml_parser_delete(&parser);
    return problem;
}

DataProblem checkDelimited(const QString &text, QChar delimiter)
{
    bool quoted = false;
    bool fieldStart = true;
    qsizetype opened = 0;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (quoted) {
            if (c != u'"')
                continue;
            if (i + 1 < text.size() && text.at(i + 1) == u'"') {
                ++i; // "" is a quote inside the field
                continue;
            }
            quoted = false;
            // A closing quote ends the field.
            if (i + 1 < text.size()) {
                const QChar next = text.at(i + 1);
                if (next != delimiter && next != u'\n' && next != u'\r')
                    return at(text, i + 1,
                              QObject::tr("text after a closing quote, before the next field"));
            }
            fieldStart = false;
        } else if (c == u'"' && fieldStart) {
            quoted = true;
            opened = i;
        } else {
            fieldStart = c == delimiter || c == u'\n';
        }
    }
    if (quoted)
        return at(text, opened, QObject::tr("a quote that is never closed"));
    return {};
}

} // namespace

DataProblem checkData(const QString &kind, const QString &text)
{
    if (kind == u"json")
        return checkJson(text);
    if (kind == u"yaml")
        return checkYaml(text);
    if (kind == u"csv")
        return checkDelimited(text, u',');
    if (kind == u"tsv")
        return checkDelimited(text, u'\t');
    return {};
}

QString dataKindOf(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == u"yml")
        return u"yaml"_s;
    if (suffix == u"json" || suffix == u"yaml" || suffix == u"csv" || suffix == u"tsv")
        return suffix;
    return {};
}

} // namespace jot
