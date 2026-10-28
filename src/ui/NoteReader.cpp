#include "NoteReader.h"

#include "ThemeController.h"
#include "VaultController.h"

#include "jot/FrontMatter.h"
#include "jot/NoteScan.h"

#include <QImage>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextList>
#include <QTextTable>
#include <QTimer>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// Each heading level's size against the body text, as the editor draws them.
constexpr double kHeadingScale[] = {1.75, 1.45, 1.22, 1.08, 1.0, 1.0};

bool isCode(const QTextBlock &block)
{
    const QTextBlockFormat format = block.blockFormat();
    return format.hasProperty(QTextFormat::BlockCodeFence) || format.nonBreakableLines();
}

bool isTask(const QTextBlock &block)
{
    const auto marker = block.blockFormat().marker();
    return block.textList() && (marker == QTextBlockFormat::MarkerType::Checked ||
                                marker == QTextBlockFormat::MarkerType::Unchecked);
}

} // namespace

NoteReader::NoteReader(QObject *parent) : QObject(parent)
{
    connect(this, &NoteReader::changed, this, &NoteReader::schedule);
    connect(ThemeController::instance(), &ThemeController::changed, this, &NoteReader::schedule);
}

void NoteReader::setDocument(QQuickTextDocument *document)
{
    if (m_document == document)
        return;
    m_document = document;
    Q_EMIT changed();
}

void NoteReader::setText(const QString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    Q_EMIT changed();
}

void NoteReader::setFontFamily(const QString &family)
{
    if (m_fontFamily == family)
        return;
    m_fontFamily = family;
    Q_EMIT changed();
}

void NoteReader::setPixelSize(int size)
{
    if (m_pixelSize == size)
        return;
    m_pixelSize = size;
    Q_EMIT changed();
}

void NoteReader::setWidth(int width)
{
    if (m_width == width)
        return;
    m_width = width;
    Q_EMIT changed();
}

void NoteReader::schedule()
{
    // Several properties arrive at once as the view is made; it is drawn once after them.
    if (m_pending)
        return;
    m_pending = true;
    QTimer::singleShot(0, this, [this] {
        m_pending = false;
        render();
    });
}

void NoteReader::render()
{
    QTextDocument *doc = m_document ? m_document->textDocument() : nullptr;
    if (!doc)
        return;
    const NoteParts parts = splitFrontMatter(m_text);
    const QString markdown = readingMarkdown(parts.body, [](const QString &target) {
        const VaultController *vault = VaultController::instance();
        const QString path = vault ? vault->attachmentPath(target, true) : QString();
        return path.isEmpty() ? QString() : QUrl::fromLocalFile(path).toString();
    });
    doc->setUndoRedoEnabled(false);
    doc->setMarkdown(markdown, QTextDocument::MarkdownDialectGitHub);

    m_tasks.clear();
    for (const NoteScan::Task &task : scanNote(parts.body, bodyFirstLine(m_text, parts.body)).tasks)
        m_tasks.append({task.line, task.text});
    m_taskBlocks.clear();
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        if (isTask(block))
            m_taskBlocks.append(block.blockNumber());
    }
    // A checkbox Qt draws that the scanner does not count, such as one in a quote, would tick
    // the wrong line, so none can be ticked.
    if (m_taskBlocks.size() != m_tasks.size())
        m_taskBlocks.clear();

    style();
    placeImages();
}

void NoteReader::style()
{
    QTextDocument *doc = m_document->textDocument();
    const ThemeController &theme = *ThemeController::instance();
    const QString family = m_fontFamily.isEmpty() ? theme.fontFamily() : m_fontFamily;
    QFont body(family);
    body.setPixelSize(m_pixelSize);
    doc->setDefaultFont(body);

    QTextCursor cursor(doc);
    cursor.beginEditBlock();
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        const QTextBlockFormat format = block.blockFormat();
        QTextCursor line(block);
        line.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QTextCharFormat look;
        if (const int level = format.headingLevel(); level > 0) {
            look.setProperty(QTextFormat::FontPixelSize,
                             int(m_pixelSize * kHeadingScale[std::min(level, 6) - 1] + 0.5));
            look.setFontWeight(theme.weightBold());
            if (level <= 2 && !theme.displayFontFamily().isEmpty())
                look.setFontFamilies({theme.displayFontFamily()});
            QTextBlockFormat spaced = format;
            spaced.setTopMargin(m_pixelSize * 0.9);
            spaced.setBottomMargin(m_pixelSize * 0.3);
            line.setBlockFormat(spaced);
        } else if (isCode(block)) {
            look.setFontFamilies({theme.monoFontFamily()});
            look.setFontFixedPitch(true);
        } else if (!QTextCursor(block).currentTable()) {
            if (format.intProperty(QTextFormat::BlockQuoteLevel) > 0)
                look.setForeground(theme.textMuted());
            // Paragraphs apart, and the items of a list close together, though apart from
            // what follows the list.
            const QTextBlock next = block.next();
            const bool inList =
                block.textList() && next.isValid() && next.textList() == block.textList();
            QTextBlockFormat spaced = format;
            spaced.setBottomMargin(inList ? m_pixelSize * 0.2 : m_pixelSize * 0.7);
            line.setBlockFormat(spaced);
        }
        if (look.propertyCount() > 0)
            line.mergeCharFormat(look);

        // Links and inline code, which Qt marks on the text itself.
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            const QTextCharFormat own = fragment.charFormat();
            QTextCharFormat run;
            if (own.isAnchor()) {
                run.setForeground(theme.accent());
                run.setFontUnderline(true);
            } else if (own.fontFixedPitch() && !format.nonBreakableLines()) {
                run.setFontFamilies({theme.monoFontFamily()});
                run.setBackground(theme.surfaceAlt());
            } else {
                continue;
            }
            QTextCursor span(doc);
            span.setPosition(fragment.position());
            span.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
            span.mergeCharFormat(run);
        }
    }
    // Code blocks on the raised surface. The view paints a frame's background but not a
    // block's, so each run of code lines goes into a frame of its own, last first.
    QList<std::pair<int, int>> runs;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        if (!isCode(block))
            continue;
        const int start = block.position();
        while (block.next().isValid() && isCode(block.next()))
            block = block.next();
        runs.append({start, block.position() + block.length() - 1});
    }
    for (auto run = runs.crbegin(); run != runs.crend(); ++run) {
        QTextCursor code(doc);
        code.setPosition(run->first);
        code.setPosition(run->second, QTextCursor::KeepAnchor);
        QTextFrameFormat box;
        box.setBackground(theme.surfaceAlt());
        box.setPadding(m_pixelSize * 0.6);
        box.setBorder(0);
        box.setBottomMargin(m_pixelSize * 0.7);
        code.insertFrame(box);
    }
    // Tables in the theme's borders, close around their cells.
    for (QTextFrame *frame : doc->rootFrame()->childFrames()) {
        if (auto *table = qobject_cast<QTextTable *>(frame)) {
            QTextTableFormat format = table->format();
            format.setBorderBrush(theme.border());
            format.setBorderCollapse(true);
            format.setBorder(1);
            format.setCellPadding(m_pixelSize * 0.4);
            format.setBottomMargin(m_pixelSize * 0.7);
            table->setFormat(format);
            for (int row = 0; row < table->rows(); ++row) {
                for (int column = 0; column < table->columns(); ++column) {
                    const QTextTableCell cell = table->cellAt(row, column);
                    // Qt starts a table that follows a quote with an empty line in its first
                    // cell, which would push that cell's text below the rest of the row.
                    QTextCursor first = cell.firstCursorPosition();
                    if (first.block().text().isEmpty() &&
                        first.block().next().position() < cell.lastCursorPosition().position()) {
                        first.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
                        first.removeSelectedText();
                    }
                    if (row == 0) {
                        QTextCursor heading = cell.firstCursorPosition();
                        heading.setPosition(cell.lastCursorPosition().position(),
                                            QTextCursor::KeepAnchor);
                        QTextCharFormat bold;
                        bold.setFontWeight(theme.weightBold());
                        heading.mergeCharFormat(bold);
                    }
                }
            }
        }
    }
    cursor.endEditBlock();
}

void NoteReader::placeImages()
{
    QTextDocument *doc = m_document->textDocument();
    const VaultController *vault = VaultController::instance();
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.charFormat().isImageFormat())
                continue;
            QTextImageFormat format = fragment.charFormat().toImageFormat();
            const QUrl url(format.name());
            const QString path = url.isLocalFile() ? url.toLocalFile()
                                 : vault           ? vault->attachmentPath(format.name(), false)
                                                   : QString();
            const QImage image(path);
            if (image.isNull())
                continue;
            const QImage shown = image.width() > m_width
                                     ? image.scaledToWidth(m_width, Qt::SmoothTransformation)
                                     : image;
            doc->addResource(QTextDocument::ImageResource, url, shown);
            format.setWidth(shown.width());
            format.setHeight(shown.height());
            QTextCursor span(doc);
            span.setPosition(fragment.position());
            span.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
            span.setCharFormat(format);
        }
    }
}

QVariantMap NoteReader::taskAt(int position) const
{
    QTextDocument *doc = m_document ? m_document->textDocument() : nullptr;
    if (!doc)
        return {};
    const QTextBlock block = doc->findBlock(position);
    const qsizetype index = m_taskBlocks.indexOf(block.blockNumber());
    if (index < 0)
        return {};
    return {{u"line"_s, m_tasks.at(index).line},
            {u"done"_s, block.blockFormat().marker() == QTextBlockFormat::MarkerType::Checked},
            {u"text"_s, m_tasks.at(index).text}};
}

} // namespace jot
