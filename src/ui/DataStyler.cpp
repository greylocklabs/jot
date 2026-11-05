#include "DataStyler.h"

#include "CodeHighlighter.h"
#include "ThemeController.h"

#include <QAbstractTextDocumentLayout>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextDocument>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// How long typing pauses before the text is checked again.
constexpr int kCheckDelayMs = 400;
/// Past this, brackets are not matched, so moving the cursor stays quick.
constexpr qsizetype kBracketLimit = 2 * 1024 * 1024;

} // namespace

/// Colors JSON and YAML through KSyntaxHighlighting, and each column of CSV and TSV in a color
/// of its own, so a row's fields can be told apart.
class DataHighlighter : public QSyntaxHighlighter
{
public:
    explicit DataHighlighter(QTextDocument *document) : QSyntaxHighlighter(document) {}

    void setLook(const QString &kind, const CodeColors &colors, int boldWeight)
    {
        m_kind = kind;
        m_colors = colors;
        m_code.setColors(colors, boldWeight);
        rehighlight();
    }

protected:
    void highlightBlock(const QString &text) override
    {
        if (m_kind == u"csv" || m_kind == u"tsv") {
            highlightFields(text, m_kind == u"csv" ? u',' : u'\t');
            return;
        }
        const auto *above =
            dynamic_cast<const CodeHighlighter::Line *>(currentBlock().previous().userData());
        auto *line = new CodeHighlighter::Line;
        line->language = m_kind == u"json" ? u"JSON"_s : u"YAML"_s;
        if (above)
            line->state = above->state;
        for (const CodeHighlighter::Span &span :
             m_code.highlight(text, line->language, line->state))
            setFormat(span.start, span.length, span.format);
        // A changed parse state makes the next line highlight again.
        setCurrentBlockState(int(qHash(line->state) & 0x7fffffff));
        setCurrentBlockUserData(line);
    }

private:
    /// The block state holds the column a line starts in, doubled, plus one inside quotes, so a
    /// quoted field running over lines carries on.
    void highlightFields(const QString &text, QChar delimiter)
    {
        const QList<QColor> palette = {m_colors.keyword, m_colors.string, m_colors.number,
                                       m_colors.type};
        const int previous = std::max(previousBlockState(), 0);
        int column = previous / 2;
        bool quoted = previous % 2 == 1;
        int start = 0;
        const auto paint = [&](int end) {
            QTextCharFormat format;
            format.setForeground(palette.at(column % palette.size()));
            setFormat(start, end - start, format);
        };
        for (int i = 0; i < text.size(); ++i) {
            const QChar c = text.at(i);
            if (c == u'"') {
                quoted = !quoted;
            } else if (c == delimiter && !quoted) {
                paint(i);
                ++column;
                start = i + 1;
            }
        }
        paint(int(text.size()));
        setCurrentBlockState(quoted ? column * 2 + 1 : 0);
    }

    QString m_kind;
    CodeColors m_colors;
    CodeHighlighter m_code;
};

DataStyler::DataStyler(QObject *parent) : QObject(parent)
{
    m_check.setSingleShot(true);
    m_check.setInterval(kCheckDelayMs);
    connect(&m_check, &QTimer::timeout, this, &DataStyler::checkNow);
    connect(ThemeController::instance(), &ThemeController::changed, this, &DataStyler::restyle);
}

DataStyler::~DataStyler()
{
    delete m_highlighter;
}

void DataStyler::setDocument(QQuickTextDocument *document)
{
    if (m_document == document)
        return;
    if (m_document && m_document->textDocument())
        disconnect(m_document->textDocument(), nullptr, &m_check, nullptr);
    delete m_highlighter;
    m_document = document;
    if (m_document && m_document->textDocument()) {
        m_highlighter = new DataHighlighter(m_document->textDocument());
        connect(m_document->textDocument(), &QTextDocument::contentsChanged, &m_check,
                qOverload<>(&QTimer::start));
    }
    restyle();
    checkNow();
    Q_EMIT documentChanged();
}

void DataStyler::setKind(const QString &kind)
{
    if (m_kind == kind)
        return;
    m_kind = kind;
    restyle();
    checkNow();
    Q_EMIT kindChanged();
}

void DataStyler::restyle()
{
    if (!m_highlighter)
        return;
    const ThemeController &theme = *ThemeController::instance();
    m_highlighter->setLook(m_kind, codeColors(theme.spec()), theme.weightBold());
}

QVariantMap DataStyler::problem() const
{
    if (m_problem.ok())
        return {};
    return {{u"line"_s, m_problem.line},
            {u"column"_s, m_problem.column},
            {u"message"_s, m_problem.message}};
}

void DataStyler::checkNow()
{
    m_check.stop();
    const QString text = m_document && m_document->textDocument()
                             ? m_document->textDocument()->toPlainText()
                             : u""_s;
    const DataProblem found = m_document ? checkData(m_kind, text) : DataProblem();
    if (found.line == m_problem.line && found.column == m_problem.column &&
        found.position == m_problem.position && found.message == m_problem.message)
        return;
    m_problem = found;
    Q_EMIT problemChanged();
}

QVariantList DataStyler::lineTops(qreal top, qreal bottom) const
{
    QVariantList lines;
    if (!m_document || !m_document->textDocument())
        return lines;
    QTextDocument *document = m_document->textDocument();
    QAbstractTextDocumentLayout *layout = document->documentLayout();
    int number = 1;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next(), ++number) {
        const QRectF box = layout->blockBoundingRect(block);
        if (box.bottom() < top)
            continue;
        if (box.top() > bottom)
            break;
        lines.append(QVariantMap{{u"number"_s, number}, {u"y"_s, box.top()}});
    }
    return lines;
}

QVariantList DataStyler::brackets(int position) const
{
    if (!m_document || !m_document->textDocument() || m_kind == u"csv" || m_kind == u"tsv")
        return {};
    const QString text = m_document->textDocument()->toPlainText();
    if (text.size() > kBracketLimit)
        return {};
    const auto isBracket = [&text](qsizetype at) {
        return at >= 0 && at < text.size() && u"{}[]"_s.contains(text.at(at));
    };
    // The bracket under the cursor, else the one just before it.
    const qsizetype wanted = isBracket(position) ? position : position - 1;
    if (!isBracket(wanted))
        return {};

    // Pairs every bracket outside strings and comments, then looks up the one wanted.
    const bool yaml = m_kind == u"yaml";
    QList<qsizetype> open;
    QChar quote;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (!quote.isNull()) {
            if (c == u'\\' && quote == u'"')
                ++i;
            else if (c == quote)
                quote = QChar();
            continue;
        }
        // In YAML a quote starts a string only where a value can start, so "Mara's" is plain.
        const bool valueStart = !yaml || i == 0 || u" \t\n[{,:-"_s.contains(text.at(i - 1));
        if ((c == u'"' || (yaml && c == u'\'')) && valueStart) {
            quote = c;
        } else if (yaml && c == u'#' && (i == 0 || text.at(i - 1).isSpace())) {
            const qsizetype end = text.indexOf(u'\n', i);
            i = end < 0 ? text.size() : end;
        } else if (c == u'{' || c == u'[') {
            open.append(i);
        } else if (c == u'}' || c == u']') {
            if (open.isEmpty())
                continue;
            const qsizetype from = open.takeLast();
            const bool pair = (text.at(from) == u'{') == (c == u'}');
            if (pair && (from == wanted || i == wanted))
                return {int(from), int(i)};
        }
    }
    return {};
}

} // namespace jot
