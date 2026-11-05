#include "jot/Diagram.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QHash>
#include <QImageReader>
#include <QJSEngine>
#include <QLoggingCategory>
#include <QPointer>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtMath>

#include <algorithm>
#include <array>
#include <optional>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// Pele's text measurer: how wide text is in the editor's own fonts, so labels fit their boxes.
class TextWidths : public QObject
{
    Q_OBJECT

public:
    void setFamilies(const QString &family, const QString &mono)
    {
        if (family == m_family && mono == m_mono)
            return;
        m_family = family;
        m_mono = mono;
        m_metrics.fill(std::nullopt);
    }

    /// `style` is Pele's bit set: 1 bold, 2 italic, 4 monospace.
    Q_INVOKABLE double width(const QString &text, double size, int style)
    {
        const int index = style & 7;
        std::optional<QFontMetricsF> &metrics = m_metrics.at(index);
        if (!metrics) {
            QFont font((style & 4) ? m_mono : m_family);
            // Measured large and scaled, since pixel sizes are whole numbers.
            font.setPixelSize(kMeasured);
            font.setBold(style & 1);
            font.setItalic(style & 2);
            metrics.emplace(font);
        }
        return metrics->horizontalAdvance(text) * size / kMeasured;
    }

private:
    static constexpr int kMeasured = 100;
    QString m_family;
    QString m_mono;
    std::array<std::optional<QFontMetricsF>, 8> m_metrics;
};

/// Quiets the engine's notes on Pele's minified code, which declares names after the
/// functions that use them, while Pele runs.
class QuietEngine
{
public:
    QuietEngine() { s_previous = QLoggingCategory::installFilter(&filter); }
    ~QuietEngine() { QLoggingCategory::installFilter(s_previous); }
    QuietEngine(const QuietEngine &) = delete;
    QuietEngine &operator=(const QuietEngine &) = delete;

private:
    static void filter(QLoggingCategory *category)
    {
        if (qstrcmp(category->categoryName(), "qt.qml.usedbeforedeclared") == 0)
            category->setEnabled(QtWarningMsg, false);
        else if (s_previous)
            s_previous(category);
    }
    static inline QLoggingCategory::CategoryFilter s_previous = nullptr;
};

/// The engine Pele runs in, with the diagrams drawn so far and the folder they are saved in.
class Engine : public QObject
{
public:
    explicit Engine(QObject *parent) : QObject(parent)
    {
        // Without Qt's SVG plugin a drawing could not be shown, so diagrams stay as code.
        if (!QImageReader::supportedImageFormats().contains("svg")) {
            m_error = u"Qt's SVG image plugin is not installed."_s;
            return;
        }
        QFile file(u":/jot/pele/pele.js"_s);
        if (!file.open(QIODevice::ReadOnly)) {
            m_error = u"Jot was built without its diagram renderer."_s;
            return;
        }
        const QuietEngine quiet;
        const QJSValue loaded = m_js.evaluate(QString::fromUtf8(file.readAll()), file.fileName());
        m_render = m_js.globalObject().property(u"pele"_s).property(u"render"_s);
        if (loaded.isError() || !m_render.isCallable()) {
            m_error = u"The diagram renderer did not start: "_s + loaded.toString();
            return;
        }
        QJSEngine::setObjectOwnership(&m_widths, QJSEngine::CppOwnership);
        m_measurer = m_js.newQObject(&m_widths);
    }

    Diagram draw(const QString &source, const DiagramStyle &style);

private:
    QString save(const QString &svg);

    // Before the engine, so it outlasts the engine's wrapper around it.
    TextWidths m_widths;
    QJSEngine m_js;
    QJSValue m_render;
    QJSValue m_measurer;
    QString m_error;
    QTemporaryDir m_dir;
    QHash<QByteArray, Diagram> m_drawn;
};

Engine &engine()
{
    static QPointer<Engine> instance;
    if (!instance)
        instance = new Engine(QCoreApplication::instance());
    return *instance;
}

QString familyList(const QString &family, const QString &fallback)
{
    QString name = family;
    name.remove(QRegularExpression(uR"(['"<>&;])"_s));
    return name.isEmpty() ? fallback : u"'"_s + name + u"', "_s + fallback;
}

/// Gives `value` with each `var(--name, fallback)` in it replaced from `vars`.
QString resolve(QStringView value, const QHash<QString, QString> &vars, int depth = 0)
{
    QString out;
    qsizetype from = 0;
    while (from < value.size()) {
        const qsizetype at = value.indexOf(u"var(", from);
        if (at < 0) {
            out += value.mid(from);
            break;
        }
        out += value.mid(from, at - from);
        qsizetype end = at + 4;
        for (int open = 1; end < value.size(); ++end) {
            if (value[end] == u'(')
                ++open;
            else if (value[end] == u')' && --open == 0)
                break;
        }
        const QStringView inside = value.mid(at + 4, end - at - 4);
        const qsizetype comma = inside.indexOf(u',');
        const QString name = (comma < 0 ? inside : inside.left(comma)).trimmed().toString();
        const auto found = vars.constFind(name);
        const QStringView chosen = found != vars.cend() ? QStringView(*found)
                                   : comma < 0          ? QStringView()
                                                        : inside.mid(comma + 1).trimmed();
        if (depth < 8)
            out += resolve(chosen, vars, depth + 1);
        from = end + 1;
    }
    return out;
}

/// Resolves custom properties inside tags only, so a label that says var(...) keeps it. Pele
/// escapes < and > in text and attribute values, so tags end at the first >.
QString resolveInTags(QStringView svg, const QHash<QString, QString> &vars)
{
    QString out;
    qsizetype from = 0;
    while (from < svg.size()) {
        const qsizetype open = svg.indexOf(u'<', from);
        if (open < 0) {
            out += svg.mid(from);
            break;
        }
        out += svg.mid(from, open - from);
        const qsizetype close = svg.indexOf(u'>', open);
        const qsizetype end = close < 0 ? svg.size() : close + 1;
        out += resolve(svg.mid(open, end - open), vars);
        from = end;
    }
    return out;
}

using Attributes = QList<std::pair<QString, QString>>;

Attributes attributesOf(QStringView tag)
{
    static const QRegularExpression attribute(uR"re(([\w:-]+)="([^"]*)")re"_s);
    Attributes found;
    for (const QRegularExpressionMatch &m : attribute.globalMatchView(tag))
        found.append({m.captured(1), m.captured(2)});
    return found;
}

QString textTag(Attributes attributes, const Attributes &over)
{
    for (const auto &[name, value] : over) {
        const auto same = std::find_if(attributes.begin(), attributes.end(),
                                       [&name](const auto &each) { return each.first == name; });
        if (same != attributes.end())
            same->second = value;
        else
            attributes.append({name, value});
    }
    QString tag = u"<text"_s;
    for (const auto &[name, value] : attributes)
        tag += u' ' + name + u"=\""_s + value + u'"';
    return tag + u'>';
}

/// Qt places a tspan where its text runs on to, never at its own x and y, so each line of a
/// label that sets them becomes a text of its own.
QString splitLines(const QString &svg)
{
    static const QRegularExpression text(uR"re(<text\b([^>]*)>(.*?)</text>)re"_s,
                                         QRegularExpression::DotMatchesEverythingOption);
    QString out;
    qsizetype from = 0;
    for (const QRegularExpressionMatch &m : text.globalMatch(svg)) {
        out += QStringView(svg).mid(from, m.capturedStart() - from);
        from = m.capturedEnd();
        const Attributes outer = attributesOf(m.capturedView(1));
        const QStringView inside = m.capturedView(2);
        // The top-level tspans, each with its tag's attributes and what it holds.
        struct Run
        {
            Attributes attributes;
            QString whole;
            QString inner;
        };
        QList<Run> runs;
        bool placed = false;
        qsizetype at = 0;
        while (at < inside.size()) {
            const qsizetype start = inside.indexOf(u"<tspan", at);
            if (start < 0) {
                if (!inside.mid(at).trimmed().isEmpty())
                    runs.append({{}, inside.mid(at).toString(), inside.mid(at).toString()});
                break;
            }
            if (start > at && !inside.mid(at, start - at).trimmed().isEmpty())
                runs.append({{},
                             inside.mid(at, start - at).toString(),
                             inside.mid(at, start - at).toString()});
            const qsizetype open = inside.indexOf(u'>', start);
            qsizetype end = open + 1;
            for (int depth = 1; depth > 0 && end < inside.size();) {
                const qsizetype next = inside.indexOf(u"<tspan", end);
                const qsizetype close = inside.indexOf(u"</tspan>", end);
                if (close < 0) {
                    end = inside.size();
                    break;
                }
                if (next >= 0 && next < close) {
                    ++depth;
                    end = next + 6;
                } else {
                    --depth;
                    end = close + 8;
                }
            }
            if (open < 0 || end > inside.size()) {
                runs.clear();
                break;
            }
            Run run{attributesOf(inside.mid(start, open - start)),
                    inside.mid(start, end - start).toString(),
                    inside.mid(open + 1, end - open - 9).toString()};
            placed = placed ||
                     std::any_of(run.attributes.cbegin(), run.attributes.cend(),
                                 [](const auto &a) { return a.first == u"x" || a.first == u"y"; });
            runs.append(run);
            at = end;
        }
        if (!placed || runs.isEmpty()) {
            out += m.captured(0);
            continue;
        }
        // A run without a place of its own carries on the line before it.
        bool first = true;
        for (const Run &run : runs) {
            const bool starts =
                std::any_of(run.attributes.cbegin(), run.attributes.cend(),
                            [](const auto &a) { return a.first == u"x" || a.first == u"y"; });
            if (starts || first) {
                if (!first)
                    out += u"</text>"_s;
                out += textTag(outer, run.attributes) + run.inner;
                first = false;
            } else {
                out += run.whole;
            }
        }
        out += u"</text>"_s;
    }
    out += QStringView(svg).mid(from);
    return out;
}

/// Writes out what Pele leaves to CSS, which Qt's SVG renderer does not read: custom
/// properties, the root's font shorthand, and corner radii given as style.
QString flatten(const QString &svg, const DiagramStyle &style)
{
    QHash<QString, QString> vars;
    const auto color = [&vars](const char *name, const QColor &value) {
        if (value.isValid())
            vars.insert(QString::fromLatin1(name), value.name(QColor::HexRgb));
    };
    color("--pele-bg", style.background);
    color("--pele-fg", style.text);
    color("--pele-line", style.text);
    color("--pele-muted", style.muted);
    color("--pele-surface", style.surface);
    color("--pele-surface-alt", style.surfaceAlt);
    color("--pele-border", style.border);
    color("--pele-accent", style.accent);
    const QString family = familyList(style.fontFamily, u"sans-serif"_s);
    vars.insert(u"--pele-font"_s, family);
    vars.insert(u"--pele-font-mono"_s, familyList(style.monoFamily, u"monospace"_s));

    const qsizetype rootEnd = svg.indexOf(u'>');
    if (!svg.startsWith(u"<svg") || rootEnd < 0)
        return {};
    QString root = svg.left(rootEnd);
    static const QRegularExpression styleAttribute(uR"re( style="([^"]*)")re"_s);
    QString textColor;
    if (const QRegularExpressionMatch m = styleAttribute.match(root); m.hasMatch()) {
        const QStringList declarations = m.captured(1).split(u';', Qt::SkipEmptyParts);
        // The root's own properties come first, as they are read in order.
        for (const QString &declaration : declarations) {
            const qsizetype colon = declaration.indexOf(u':');
            const QString name = declaration.left(colon).trimmed();
            if (colon > 0 && name.startsWith(u"--") && !vars.contains(name))
                vars.insert(name, declaration.mid(colon + 1).trimmed());
        }
        for (const QString &declaration : declarations) {
            const qsizetype colon = declaration.indexOf(u':');
            if (colon > 0 && declaration.left(colon).trimmed() == u"color")
                textColor = resolve(declaration.mid(colon + 1).trimmed(), vars);
        }
        root.remove(m.capturedStart(), m.capturedLength());
    }
    root += u" font-family=\""_s + family + u"\" font-size=\""_s +
            QString::number(style.pixelSize) + u'"';
    if (!textColor.isEmpty())
        root += u" color=\""_s + textColor + u'"';

    QString body = resolveInTags(QStringView(svg).mid(rootEnd), vars);
    static const QRegularExpression radius(uR"( style="rx:\s*([\d.]+)(?:px)?;?")"_s);
    body.replace(radius, uR"( rx="\1" ry="\1")"_s);
    return resolve(root, vars) + splitLines(body);
}

Diagram Engine::draw(const QString &source, const DiagramStyle &style)
{
    if (!m_error.isEmpty()) {
        Diagram failed;
        failed.error = m_error;
        return failed;
    }
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(source.toUtf8());
    for (const QString &part :
         {style.background.name(), style.text.name(), style.muted.name(), style.surface.name(),
          style.surfaceAlt.name(), style.border.name(), style.accent.name(), style.fontFamily,
          style.monoFamily, QString::number(style.pixelSize)}) {
        hash.addData(QByteArrayView("\0", 1));
        hash.addData(part.toUtf8());
    }
    const QByteArray key = hash.result();
    if (const auto found = m_drawn.constFind(key); found != m_drawn.cend())
        return *found;

    const QString family = style.fontFamily.isEmpty()
                               ? QFontDatabase::systemFont(QFontDatabase::GeneralFont).family()
                               : style.fontFamily;
    const QString mono = style.monoFamily.isEmpty()
                             ? QFontDatabase::systemFont(QFontDatabase::FixedFont).family()
                             : style.monoFamily;
    m_widths.setFamilies(family, mono);
    QJSValue options = m_js.newObject();
    options.setProperty(u"measurer"_s, m_measurer);
    options.setProperty(u"fontSize"_s, style.pixelSize);
    // A diagram is a picture in a note: nothing in it reaches out to the web.
    options.setProperty(u"links"_s, false);
    options.setProperty(u"images"_s, false);
    const QJSValue result = [&] {
        const QuietEngine quiet;
        return m_render.call({source, options});
    }();

    Diagram diagram;
    if (result.isError()) {
        diagram.error = result.property(u"message"_s).toString();
    } else {
        const QString svg = flatten(result.property(u"svg"_s).toString(), style);
        diagram.size = QSize(qCeil(result.property(u"width"_s).toNumber()),
                             qCeil(result.property(u"height"_s).toNumber()));
        diagram.path = svg.isEmpty() ? QString() : save(svg);
        if (diagram.path.isEmpty())
            diagram.error = u"The diagram could not be saved to show."_s;
    }
    // Kept small, since each edit to a diagram draws it anew.
    if (m_drawn.size() >= 256)
        m_drawn.clear();
    m_drawn.insert(key, diagram);
    return diagram;
}

QString Engine::save(const QString &svg)
{
    if (!m_dir.isValid())
        return {};
    const QByteArray bytes = svg.toUtf8();
    const QString path = m_dir.filePath(
        QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex()) +
        u".svg"_s);
    QFile file(path);
    if (!file.exists() && (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()))
        return {};
    return path;
}

} // namespace

bool isDiagramLanguage(QStringView language)
{
    return language.compare(u"mermaid", Qt::CaseInsensitive) == 0;
}

Diagram drawDiagram(const QString &source, const DiagramStyle &style)
{
    return engine().draw(source, style);
}

} // namespace jot

#include "Diagram.moc"
