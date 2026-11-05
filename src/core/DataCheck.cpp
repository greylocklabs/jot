#include "jot/DataCheck.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QObject>

#include <yaml.h>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// Characters in `text`, a pair of UTF-16 units for an emoji counting once.
int characters(QStringView text)
{
    int count = 0;
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (!(text.at(i).isLowSurrogate() && i > 0 && text.at(i - 1).isHighSurrogate()))
            ++count;
    }
    return count;
}

/// The problem at UTF-16 position `index` in `text`.
DataProblem at(const QString &text, qsizetype index, const QString &message)
{
    index = std::clamp<qsizetype>(index, 0, text.size());
    const qsizetype lineStart = text.lastIndexOf(u'\n', index - 1) + 1;
    return {int(text.left(index).count(u'\n')) + 1,
            characters(QStringView(text).sliced(lineStart, index - lineStart)) + 1, message, index};
}

/// The problem on `line` and at character `column`, both from 0, as libyaml counts them.
DataProblem atCharacter(const QString &text, int line, int column, const QString &message)
{
    qsizetype index = 0;
    for (int l = 0; l < line && index >= 0; ++l) {
        index = text.indexOf(u'\n', index);
        if (index >= 0)
            ++index;
    }
    if (index < 0)
        index = text.size();
    for (int c = 0; c < column && index < text.size() && text.at(index) != u'\n'; ++c)
        index += text.at(index).isHighSurrogate() && index + 1 < text.size() ? 2 : 1;
    return at(text, index, message);
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
            problem =
                atCharacter(text, int(parser.problem_mark.line), int(parser.problem_mark.column),
                            message.isEmpty() ? QObject::tr("not valid YAML") : message);
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
