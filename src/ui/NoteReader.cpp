#include "NoteReader.h"

#include "MarkdownHighlighter.h"
#include "MathRenderer.h"
#include "ThemeController.h"
#include "VaultController.h"

#include "jot/Diagram.h"
#include "jot/FrontMatter.h"
#include "jot/NoteScan.h"
#include "jot/TextFind.h"
#include "jot/WebVideo.h"

#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextFrame>
#include <QTextList>
#include <QTextTable>
#include <QTimer>
#include <QUrl>
#include <QtMath>

#include <array>
#include <cmath>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

/// Gives `markdown` with each diagram fence drawn as an image of it; one that cannot be drawn
/// stays as its code.
QString drawDiagrams(const QString &markdown, const DiagramStyle &style)
{
    static const QRegularExpression opening(uR"(^ {0,3}(`{3,}|~{3,})\s*([^\s`{]*))"_s);
    const QStringList lines = markdown.split(u'\n');
    QStringList out;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        const QRegularExpressionMatch fence = opening.match(lines.at(i));
        if (!fence.hasMatch()) {
            out.append(lines.at(i));
            continue;
        }
        // The fence closes on a line of the same marks, at least as many, and nothing else.
        const QString marks = fence.captured(1);
        const QRegularExpression closing(u"^ {0,3}"_s + QRegularExpression::escape(marks.left(1)) +
                                         u"{%1,}\\s*$"_s.arg(marks.size()));
        qsizetype end = i + 1;
        while (end < lines.size() && !closing.match(lines.at(end)).hasMatch())
            ++end;
        const Diagram diagram = end < lines.size() && isDiagramLanguage(fence.capturedView(2))
                                    ? drawDiagram(lines.mid(i + 1, end - i - 1).join(u'\n'), style)
                                    : Diagram();
        if (diagram.ok()) {
            out.append(u"![Diagram](<%1>)"_s.arg(diagram.path));
        } else {
            for (qsizetype k = i; k <= end && k < lines.size(); ++k)
                out.append(lines.at(k));
        }
        i = end;
    }
    return out.join(u'\n');
}

bool isCode(const QTextBlock &block)
{
    const QTextBlockFormat format = block.blockFormat();
    return format.hasProperty(QTextFormat::BlockCodeFence) || format.nonBreakableLines();
}

int quoteLevel(const QTextBlock &block)
{
    return block.blockFormat().intProperty(QTextFormat::BlockQuoteLevel);
}

bool isTask(const QTextBlock &block)
{
    const auto marker = block.blockFormat().marker();
    return block.textList() && (marker == QTextBlockFormat::MarkerType::Checked ||
                                marker == QTextBlockFormat::MarkerType::Unchecked);
}

/// A task's box as the app draws its own checkboxes, `side` pixels square, with `gap` clear
/// pixels to its right.
QImage checkBox(bool done, int side, int gap, const ThemeController &theme)
{
    const qreal scale = std::max(2.0, qGuiApp->devicePixelRatio());
    QImage image(QSizeF((side + gap) * scale, side * scale).toSize(),
                 QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter paint(&image);
    paint.setRenderHint(QPainter::Antialiasing);
    paint.scale(scale, scale);
    const qreal stroke = 2;
    paint.setPen(QPen(done ? theme.accent() : theme.border(), stroke));
    paint.setBrush(done ? theme.accent() : theme.surface());
    paint.drawRoundedRect(QRectF(stroke / 2, stroke / 2, side - stroke, side - stroke),
                          theme.radiusSm(), theme.radiusSm());
    if (done) {
        // The check of Glyph.qml: bars of a length, an angle and a center, in its inner square.
        const qreal inset = 2;
        const qreal inner = side - 2 * inset;
        paint.setPen(QPen(theme.accentText(), stroke, Qt::SolidLine, Qt::RoundCap));
        for (const auto &[length, angle, x, y] : {std::array<qreal, 4>{0.34, 45, 0.33, 0.6},
                                                  std::array<qreal, 4>{0.62, -50, 0.6, 0.47}}) {
            const QPointF center(inset + inner * x, inset + inner * y);
            const qreal half = (inner * length - stroke) / 2;
            const QPointF along(half * std::cos(qDegreesToRadians(angle)),
                                half * std::sin(qDegreesToRadians(angle)));
            paint.drawLine(center - along, center + along);
        }
    }
    image.setDevicePixelRatio(scale);
    return image;
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
    m_unplaced.clear();
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
    const ThemeController &theme = *ThemeController::instance();
    const int firstLine = bodyFirstLine(m_text, parts.body);
    // Folded sections are left out, outermost first, as are the to-dos in them.
    const QList<Heading> headings = headingsOf(parts.body);
    const QStringList keys = headingKeys(headings);
    QList<std::pair<qsizetype, qsizetype>> cut;
    QList<std::pair<int, int>> cutLines;
    QVariantList foldable;
    m_hiddenHeadings = QList<bool>(headings.size(), false);
    // Headings' sections and list items' lines, in the order they come; a heading is known by
    // its index, a list item by where it is.
    struct Foldable
    {
        QString key;
        qsizetype at;
        qsizetype start;
        qsizetype end;
        int heading;
    };
    QList<Foldable> foldables;
    for (qsizetype i = 0; i < headings.size(); ++i) {
        const auto [start, end] = sectionOf(parts.body, headings, i);
        foldables.append({keys.at(i), headings.at(i).position, start, end, int(i)});
    }
    for (const ListFold &list : listFolds(parts.body, keys))
        foldables.append({list.key, list.position, list.start, list.end, -1});
    std::ranges::stable_sort(foldables, {}, &Foldable::at);
    for (const Foldable &each : std::as_const(foldables)) {
        if (!cut.isEmpty() && each.at >= cut.last().first && each.at < cut.last().second) {
            if (each.heading >= 0)
                m_hiddenHeadings[each.heading] = true;
            continue;
        }
        if (each.start == each.end)
            continue;
        const bool folded =
            m_folded.contains(each.key) && (each.heading >= 0 || !m_unplaced.contains(each.key));
        foldable.append(QVariantMap{{u"key"_s, each.key},
                                    {u"index"_s, each.heading},
                                    {u"at"_s, each.at},
                                    {u"folded"_s, folded}});
        if (folded) {
            cut.append({each.start, each.end});
            const auto lineAt = [&](qsizetype position) {
                return firstLine + int(parts.body.first(position).count(u'\n'));
            };
            cutLines.append({lineAt(each.start), lineAt(each.end)});
        }
    }
    m_shown.clear();
    qsizetype from = 0;
    for (const auto &[start, end] : std::as_const(cut)) {
        m_shown += parts.body.sliced(from, start - from);
        from = end;
    }
    m_shown += parts.body.sliced(from);
    QString markdown = readingMarkdown(m_shown, [this, &theme](const QString &tex, bool display) {
        const MathRenderer::Drawn drawn =
            MathRenderer::instance().draw(tex, display, m_pixelSize, theme.text());
        return drawn.ok() ? QUrl::fromLocalFile(drawn.path).toString() : QString();
    });
    markdown = drawDiagrams(markdown, theme.diagramStyle(m_fontFamily, m_pixelSize));
    doc->setUndoRedoEnabled(false);
    // Image names resolve beside the note, as files, rather than inside the app's resources,
    // where the TextEdit would otherwise look for them itself.
    if (const VaultController *vault = VaultController::instance(); vault && vault->isOpen())
        doc->setBaseUrl(QUrl::fromLocalFile(vault->root() + u'/' + vault->current()));
    doc->setMarkdown(markdown, QTextDocument::MarkdownDialectGitHub);
    // Each list item drawn, in order, to find where a list fold's item is; before styling, which
    // draws lists its own way.
    // Kept as blocks, not positions: styling and the embeds placed after shift what follows.
    QList<QTextBlock> listBlocks;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        if (block.textList())
            listBlocks.append(block);
    }

    m_tasks.clear();
    for (const NoteScan::Task &task : scanNote(parts.body, firstLine).tasks) {
        if (std::ranges::none_of(cutLines, [&](const auto &lines) {
                return task.line >= lines.first && task.line < lines.second;
            }))
            m_tasks.append({task.line, task.text});
    }
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
    placeSounds();

    const QList<qsizetype> listStarts = listItemStarts(m_shown);
    for (QVariant &fold : foldable) {
        QVariantMap map = fold.toMap();
        const int index = map.value(u"index"_s).toInt();
        int position = -1;
        if (index >= 0) {
            position = headingPosition(index);
        } else if (listStarts.size() == listBlocks.size()) {
            // Where the item is in what is shown, with the cuts before it taken out.
            qsizetype at = map.value(u"at"_s).toLongLong();
            for (const auto &[start, end] : std::as_const(cut)) {
                if (end <= at)
                    at -= end - start;
            }
            const qsizetype item = listStarts.indexOf(at);
            if (item >= 0 && listBlocks.at(item).isValid())
                position = listBlocks.at(item).position();
        }
        const QTextBlock line = doc->findBlock(std::max(position, 0));
        map.insert(u"position"_s, position);
        map.insert(u"end"_s, line.position() + line.length() - 1);
        fold = map;
    }
    // A folded list item with no arrow to open it again is drawn unfolded instead.
    bool unplaced = false;
    for (const QVariant &fold : std::as_const(foldable)) {
        const QVariantMap map = fold.toMap();
        if (map.value(u"folded"_s).toBool() && map.value(u"position"_s).toInt() < 0 &&
            !m_unplaced.contains(map.value(u"key"_s).toString())) {
            m_unplaced.append(map.value(u"key"_s).toString());
            unplaced = true;
        }
    }
    if (unplaced) {
        render();
        return;
    }
    foldable.removeIf(
        [](const QVariant &fold) { return fold.toMap().value(u"position"_s).toInt() < 0; });
    if (foldable != m_folds) {
        m_folds = foldable;
        Q_EMIT foldsChanged();
    }
}

void NoteReader::setFolded(const QStringList &folded)
{
    if (m_folded == folded)
        return;
    m_folded = folded;
    schedule();
}

int NoteReader::headingPosition(int index) const
{
    const QTextDocument *doc = m_document ? m_document->textDocument() : nullptr;
    if (!doc || index < 0)
        return -1;
    // A heading folded away with another is not drawn; those after it are one place earlier.
    if (index < m_hiddenHeadings.size() && m_hiddenHeadings.at(index))
        return -1;
    index -= int(std::count(
        m_hiddenHeadings.cbegin(),
        m_hiddenHeadings.cbegin() + std::min<qsizetype>(index, m_hiddenHeadings.size()), true));
    // Its place among every heading drawn, nested ones in quotes and lists too.
    int drawn = -1;
    int listed = -1;
    const QList<Heading> all = allHeadingsOf(m_shown);
    for (int i = 0; i < all.size() && drawn < 0; ++i) {
        if (!all.at(i).nested && ++listed == index)
            drawn = i;
    }
    if (drawn < 0)
        return -1;
    // Should what is drawn have other headings than were found, none is the right one to go to.
    int headings = 0;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next())
        headings += block.blockFormat().headingLevel() > 0;
    if (headings != all.size())
        return -1;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        if (block.blockFormat().headingLevel() > 0 && drawn-- == 0)
            return block.position();
    }
    return -1;
}

QVariantList NoteReader::findAll(const QString &needle, bool matchCase, bool wholeWords) const
{
    QVariantList starts;
    QTextDocument *doc = m_document ? m_document->textDocument() : nullptr;
    if (!doc)
        return starts;
    for (const qsizetype start : jot::findAll(doc->toPlainText(), needle, matchCase, wholeWords))
        starts.append(int(start));
    return starts;
}

void NoteReader::setFound(const QVariantList &starts, int length, int current)
{
    QVariantList found;
    for (qsizetype i = 0; i < starts.size(); ++i) {
        const int start = starts.at(i).toInt();
        found.append(QVariantMap{
            {u"start"_s, start}, {u"end"_s, start + length}, {u"current"_s, i == current}});
    }
    if (found == m_found)
        return;
    m_found = found;
    Q_EMIT foundChanged();
}

qreal NoteReader::quoteIndent()
{
    return ThemeController::instance()->space5();
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
    // Quotes a step in for each level, past the bar the view draws, rather than Qt's wide indent.
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        if (const int level = quoteLevel(block); level > 0 && !isCode(block)) {
            QTextBlockFormat in;
            in.setLeftMargin(level * quoteIndent());
            QTextCursor(block).mergeBlockFormat(in);
        }
    }
    // Code blocks on the raised surface, which the view draws behind them: in frames of their
    // own, Qt's partial redraws drew their text again and again, bolder, around a selection.
    QVariantList runs;
    const qreal pad = codePadding();
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        if (!isCode(block))
            continue;
        QTextBlock first = block;
        // A run ends where the next code is in another quote, or out of one.
        while (block.next().isValid() && isCode(block.next()) &&
               quoteLevel(block.next()) == quoteLevel(first))
            block = block.next();
        for (QTextBlock each = first;; each = each.next()) {
            QTextBlockFormat room;
            room.setLeftMargin(pad + quoteLevel(first) * quoteIndent());
            room.setRightMargin(pad);
            // As far from what is around it as a paragraph, beyond the surface's own room.
            room.setTopMargin(each == first ? pad + m_pixelSize * 0.7 : 0);
            room.setBottomMargin(each == block ? pad + m_pixelSize * 0.7 : 0);
            QTextCursor(each).mergeBlockFormat(room);
            if (each == block)
                break;
        }
        runs.append(QVariantMap{{u"start"_s, first.position()},
                                {u"end"_s, block.position() + block.length() - 1},
                                {u"left"_s, quoteLevel(first) * quoteIndent()}});
    }
    if (runs != m_codeRuns) {
        m_codeRuns = runs;
        Q_EMIT codeRunsChanged();
    }
    // Every level of a quote, over the lines that go at least that deep, for its bar.
    QVariantList quotes;
    int deepest = 0;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next())
        deepest = std::max(deepest, quoteLevel(block));
    for (int level = 1; level <= deepest; ++level) {
        for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
            if (quoteLevel(block) < level)
                continue;
            const QTextBlock first = block;
            while (block.next().isValid() && quoteLevel(block.next()) >= level)
                block = block.next();
            quotes.append(QVariantMap{{u"start"_s, first.position()},
                                      {u"end"_s, block.position() + block.length() - 1},
                                      {u"level"_s, level}});
        }
    }
    if (quotes != m_quoteRuns) {
        m_quoteRuns = quotes;
        Q_EMIT quoteRunsChanged();
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
    drawTasks();
    cursor.endEditBlock();
}

void NoteReader::drawTasks()
{
    QTextDocument *doc = m_document->textDocument();
    const ThemeController &theme = *ThemeController::instance();
    // Qt draws a task's box as a character; each is taken out of its list and drawn as the app's
    // own box instead, hanging left of the text where the marker was.
    const int side = m_pixelSize + 2;
    const int gap = theme.space3();
    for (const int number : std::as_const(m_taskBlocks)) {
        const QTextBlock block = doc->findBlockByNumber(number);
        QTextList *list = block.textList();
        if (!list)
            continue;
        const bool done = block.blockFormat().marker() == QTextBlockFormat::MarkerType::Checked;
        const int indent = list->format().indent();
        list->remove(block);
        QTextCursor line(block);
        QTextBlockFormat hanging = block.blockFormat();
        hanging.setIndent(indent);
        hanging.setTextIndent(-(side + gap));
        line.setBlockFormat(hanging);
        if (done) {
            line.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            QTextCharFormat finished;
            finished.setForeground(theme.textMuted());
            finished.setFontStrikeOut(true);
            line.mergeCharFormat(finished);
        }
        const QString name = done ? u"jot-task:done"_s : u"jot-task:open"_s;
        doc->addResource(QTextDocument::ImageResource, QUrl(name),
                         checkBox(done, side, gap, theme));
        QTextImageFormat box;
        box.setName(name);
        box.setWidth(side + gap);
        box.setHeight(side);
        box.setVerticalAlignment(QTextCharFormat::AlignMiddle);
        QTextCursor start(block);
        start.insertImage(box);
    }
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
            if (format.name().startsWith(u"jot-task:"_s))
                continue;
            const QUrl url(format.name());
            // A path or file URL as written, else an attachment the vault finds by its name.
            const QUrl resolved = doc->baseUrl().resolved(url);
            const QString path = resolved.isLocalFile() && QFileInfo::exists(resolved.toLocalFile())
                                     ? resolved.toLocalFile()
                                 : vault ? vault->attachmentPath(format.name())
                                         : QString();
            QImageReader reader(path);
            QImage image;
            QSize shown;
            if (reader.format() == "svg" && reader.size().isValid() && !reader.size().isEmpty()) {
                // A drawing is read at twice the size it is shown, to stay sharp on a dense screen.
                const QSize natural = reader.size();
                shown = natural.width() > m_width
                            ? QSize(m_width, natural.height() * m_width / natural.width())
                            : natural;
                reader.setScaledSize(shown * 2);
                image = reader.read();
            } else if (path.startsWith(MathRenderer::folder() + u'/')) {
                // Drawn at twice its size, as math is, and shown at half, in the line's middle.
                image = QImage(path);
                shown = image.size() / 2;
                if (shown.width() > m_width)
                    shown = QSize(m_width, shown.height() * m_width / shown.width());
                format.setVerticalAlignment(QTextCharFormat::AlignMiddle);
            } else {
                image = QImage(path);
                if (image.width() > m_width)
                    image = image.scaledToWidth(m_width, Qt::SmoothTransformation);
                shown = image.size();
            }
            if (image.isNull())
                continue;
            doc->addResource(QTextDocument::ImageResource, url, image);
            format.setWidth(shown.width());
            format.setHeight(shown.height());
            QTextCursor span(doc);
            span.setPosition(fragment.position());
            span.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
            span.setCharFormat(format);
        }
    }
}

void NoteReader::copy(int start, int end) const
{
    QTextDocument *doc = m_document ? m_document->textDocument() : nullptr;
    if (!doc)
        return;
    QTextCursor selection(doc);
    selection.setPosition(start);
    selection.setPosition(end, QTextCursor::KeepAnchor);
    QString text = selection.selection().toPlainText();
    text.remove(QChar::ObjectReplacementCharacter);
    QGuiApplication::clipboard()->setText(text);
}

void NoteReader::placeSounds()
{
    QTextDocument *doc = m_document->textDocument();
    const VaultController *vault = VaultController::instance();
    QVariantList sounds;
    // A sound's player is as wide as an image can be, at most 420, and as tall as in the editor;
    // a video's is a 16:9 picture as wide as the note, at most 640, with the controls under.
    const int controls = 2 * m_pixelSize + 16;
    const QSize soundRoom(std::min(m_width, 420), controls);
    const int videoWidth = std::min(m_width, 640);
    const QSize videoRoom(videoWidth, videoWidth * 9 / 16 + controls);
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        // A line of its own: one sound's link and nothing else.
        QString href;
        bool alone = true;
        for (auto it = block.begin(); !it.atEnd() && alone; ++it) {
            const QTextFragment fragment = it.fragment();
            const QString link = fragment.charFormat().anchorHref();
            if (link.isEmpty())
                alone = fragment.text().trimmed().isEmpty();
            else if (href.isEmpty() || href == link)
                href = link;
            else
                alone = false;
        }
        const bool video = href.startsWith(u"jot-video:"_s);
        const bool onWeb = href.startsWith(u"jot-web-video:"_s);
        // A table cell has no room for a player, so the sound or video stays a link there.
        if (!alone || !(video || onWeb || href.startsWith(u"jot-sound:"_s)) || !vault ||
            QTextCursor(block).currentTable())
            continue;
        const QString target = QUrl::fromPercentEncoding(href.section(u':', 1).toUtf8());
        // A video on the web takes its picture's room, its site's player bringing controls.
        const std::optional<WebVideo> web = onWeb ? webVideo(target) : std::nullopt;
        if (onWeb && !web)
            continue;
        const QSize room = web     ? QSize(videoWidth, videoWidth * 9 / 16)
                           : video ? videoRoom
                                   : soundRoom;
        const QUrl beside = doc->baseUrl().resolved(QUrl(target));
        const QString path = web ? QString()
                             : beside.isLocalFile() && QFileInfo::exists(beside.toLocalFile())
                                 ? beside.toLocalFile()
                                 : vault->attachmentPath(target);
        if (path.isEmpty() && !web)
            continue;
        // The line keeps the player's room with a clear picture in place of the link.
        const QString name = u"jot-sound-room:%1x%2"_s.arg(room.width()).arg(room.height());
        if (doc->resource(QTextDocument::ImageResource, QUrl(name)).isNull()) {
            QImage clear(room, QImage::Format_ARGB32_Premultiplied);
            clear.fill(Qt::transparent);
            doc->addResource(QTextDocument::ImageResource, QUrl(name), clear);
        }
        QTextCursor line(block);
        line.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QTextImageFormat format;
        format.setName(name);
        format.setWidth(room.width());
        format.setHeight(room.height());
        line.insertImage(format);
        if (web)
            sounds.append(QVariantMap{{u"position"_s, block.position()},
                                      {u"width"_s, room.width()},
                                      {u"height"_s, room.height()},
                                      {u"web"_s, true},
                                      {u"site"_s, web->site},
                                      {u"link"_s, web->link},
                                      {u"embed"_s, web->embed}});
        else
            sounds.append(QVariantMap{{u"position"_s, block.position()},
                                      {u"source"_s, QUrl::fromLocalFile(path)},
                                      {u"width"_s, room.width()},
                                      {u"height"_s, room.height()},
                                      {u"video"_s, video}});
    }
    if (sounds != m_sounds) {
        m_sounds = sounds;
        Q_EMIT soundsChanged();
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
            {u"text"_s, m_tasks.at(index).text},
            {u"textStart"_s, block.position() + 1}};
}

} // namespace jot
