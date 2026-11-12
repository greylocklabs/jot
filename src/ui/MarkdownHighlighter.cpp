#include "MarkdownHighlighter.h"

#include "CodeHighlighter.h"
#include "MathRenderer.h"

#include "jot/Diagram.h"
#include "jot/FrontMatter.h"
#include "jot/TaskLine.h"
#include "jot/Vault.h"

#include <Sonnet/Speller>

#include <QFileInfo>
#include <QFontMetricsF>
#include <QImageReader>
#include <QRegularExpression>
#include <QTextDocument>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

const QRegularExpression &fencePattern()
{
    // The info string's first word names the language.
    static const QRegularExpression fence(uR"(^\s{0,3}(```+|~~~+)\s*([^\s`{]*))"_s);
    return fence;
}

int fenceState(const QString &language, const KSyntaxHighlighting::State &parse)
{
    const size_t hash = qHash(language) ^ qHash(parse);
    return int((hash & 0x0fffffff) << 2) | MarkdownHighlighter::InFence;
}

const QRegularExpression &linkPattern()
{
    static const QRegularExpression link(
        uR"((!?)\[((?:[^\[\]]|\[[^\[\]]*\])*)\]\(\s*(<[^>]*>|[^\s)]+)(?:\s+(?:"[^"]*"|'[^']*'))?\s*\))"_s);
    return link;
}

const QRegularExpression &urlPattern()
{
    static const QRegularExpression url(uR"(<?\b(https?://[^\s<>()]+[^\s<>().,;:!?'"])>?)"_s);
    return url;
}

const QRegularExpression &listPattern()
{
    static const QRegularExpression list(
        uR"(^(\s*)([-*+]|\d+[.)])(\s+)(?:(\[)([ xX])(\])(\s|$))?)"_s);
    return list;
}

struct Range
{
    int start;
    int end;
};

bool overlaps(const QList<Range> &taken, int start, int end)
{
    return std::ranges::any_of(taken,
                               [&](const Range &r) { return start < r.end && end > r.start; });
}

} // namespace

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document), m_code(std::make_unique<CodeHighlighter>())
{}

MarkdownHighlighter::~MarkdownHighlighter() = default;

void MarkdownHighlighter::setLook(const Look &look)
{
    m_look = look;
    m_code->setColors(look.code, look.boldWeight);
    rehighlight();
}

void MarkdownHighlighter::setResolver(Resolver resolver)
{
    m_resolver = std::move(resolver);
    m_sizes.clear();
    m_resolved.clear();
    rehighlight();
}

QSize MarkdownHighlighter::imageSize(const QString &path)
{
    const QString key =
        path + u'@' + QString::number(QFileInfo(path).lastModified().toMSecsSinceEpoch());
    if (const auto it = m_sizes.constFind(key); it != m_sizes.cend())
        return *it;
    const QSize size = QImageReader(path).size();
    m_sizes.insert(key, size);
    return size;
}

bool MarkdownHighlighter::imageLine(const QString &text)
{
    static const QRegularExpression line(
        uR"(^(\s*)!\[[^\]]*\]\(\s*(<[^>]*>|[^\s)]+)(?:\s+"[^"]*")?\s*\)\s*$)"_s);
    const QRegularExpressionMatch m = line.match(text);
    if (!m.hasMatch() || !m_resolver)
        return false;
    QString target = m.captured(2);
    if (target.startsWith(u'<'))
        target = target.mid(1, target.size() - 2);
    QString path = m_resolved.value(target);
    if (path.isEmpty()) {
        path = m_resolver(target);
        // Only what was found is kept, so a file added later is still found.
        if (!path.isEmpty())
            m_resolved.insert(target, path);
    }
    // A sound gets a player as wide as an image can be, at most 420.
    if (!path.isEmpty() && Vault::isAudioFile(path)) {
        if (m_cursorHere)
            return false;
        showImage(text, int(m.capturedLength(1)), path,
                  QSize(std::min(m_look.imageWidth, 420), 2 * m_look.pixelSize + 16), true);
        return true;
    }
    // A video gets a player as wide as a picture can be, a 16:9 picture with its controls under.
    if (!path.isEmpty() && Vault::isVideoFile(path)) {
        if (m_cursorHere)
            return false;
        // No taller than a picture may be, narrower to keep its shape.
        const int picture = std::min(m_look.imageWidth * 9 / 16, m_look.imageHeight);
        const int controls = 2 * m_look.pixelSize + 16;
        showImage(text, int(m.capturedLength(1)), path, QSize(picture * 16 / 9, picture + controls),
                  false, true);
        return true;
    }
    const QSize natural = path.isEmpty() ? QSize() : imageSize(path);
    if (!natural.isValid() || natural.isEmpty())
        return false;

    QSize shown = natural;
    if (shown.width() > m_look.imageWidth)
        shown = QSize(m_look.imageWidth, natural.height() * m_look.imageWidth / natural.width());
    if (shown.height() > m_look.imageHeight)
        shown = QSize(shown.width() * m_look.imageHeight / shown.height(), m_look.imageHeight);
    // On the cursor's line the link shows instead, to be edited.
    if (m_cursorHere)
        return false;
    showImage(text, int(m.capturedLength(1)), path, shown);
    return true;
}

void MarkdownHighlighter::showImage(const QString &text, int bang, const QString &path, QSize shown,
                                    bool audio, bool video)
{
    auto *image = new Image;
    image->path = path;
    image->size = shown;
    image->audio = audio;
    image->video = video;
    setCurrentBlockUserData(image);
    reserve(text, bang, shown.height());
}

void MarkdownHighlighter::reserve(const QString &text, int at, int height)
{
    // The line's one visible character is made exactly as tall as what is drawn there and
    // no wider, which reserves its room; the rest folds away.
    QFont font;
    if (!m_look.family.isEmpty())
        font.setFamily(m_look.family);
    font.setPixelSize(std::max(height, 1));
    const double lineHeight = QFontMetricsF(font).height();
    const int pixelSize = std::max(1, int(height * height / lineHeight));
    font.setPixelSize(pixelSize);
    QTextCharFormat tall;
    tall.setForeground(Qt::transparent);
    tall.setProperty(QTextFormat::FontPixelSize, pixelSize);
    tall.setFontLetterSpacingType(QFont::AbsoluteSpacing);
    tall.setFontLetterSpacing(-QFontMetricsF(font).horizontalAdvance(text.at(at)));
    setFormat(at, 1, tall);
    mark(0, at);
    mark(at + 1, int(text.size()) - at - 1);
}

bool MarkdownHighlighter::contentsLine(const QString &text)
{
    if (m_cursorHere || !isTocMarker(text))
        return false;
    const QString note = document()->toPlainText();
    auto *toc = new Toc;
    // A note without headings yet still shows a row, saying so.
    toc->rows = int(std::max<qsizetype>(headingsOf(splitFrontMatter(note).body).size(), 1));
    toc->rowHeight = int(m_look.pixelSize * 1.8 + 0.5);
    setCurrentBlockUserData(toc);
    reserve(text, int(text.indexOf(u'$')), toc->rows * toc->rowHeight);
    return true;
}

void MarkdownHighlighter::refreshContents()
{
    if (!document())
        return;
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        if (isTocMarker(block.text()))
            rehighlightBlock(block);
    }
}

std::pair<int, int> MarkdownHighlighter::diagramFence(const QTextBlock &block) const
{
    QTextBlock open = block;
    while (open.previous().isValid() && kindOf(open.previous().userState()) == InFence)
        open = open.previous();
    const QRegularExpressionMatch opening = fencePattern().match(open.text());
    if (!opening.hasMatch() || !isDiagramLanguage(opening.capturedView(2)))
        return {-1, -1};
    for (QTextBlock line = open.next(); line.isValid(); line = line.next()) {
        if (fencePattern().match(line.text()).hasMatch()) {
            // A block past the fence is in none.
            if (block.blockNumber() > line.blockNumber())
                return {-1, -1};
            return {open.blockNumber(), line.blockNumber()};
        }
    }
    return {-1, -1};
}

bool MarkdownHighlighter::diagramLine(const QString &text, int last)
{
    QStringList lines;
    for (QTextBlock line = currentBlock().next(); line.isValid() && line.blockNumber() < last;
         line = line.next())
        lines.append(line.text());
    const Diagram diagram = drawDiagram(lines.join(u'\n'), m_look.diagram);
    if (!diagram.ok())
        return false;
    // Only narrowed to fit: a diagram is read, so it is never shrunk to an image's height.
    QSize shown = diagram.size;
    if (shown.width() > m_look.imageWidth)
        shown = QSize(m_look.imageWidth,
                      diagram.size.height() * m_look.imageWidth / diagram.size.width());
    showImage(text, 0, diagram.path, shown);
    return true;
}

void MarkdownHighlighter::setCursorPosition(int position)
{
    if (!document())
        return;
    const int block = document()->findBlock(position).blockNumber();
    if (block == m_cursorBlock)
        return;
    const int before = m_cursorBlock;
    m_cursorBlock = block;
    // A diagram or display math shows as its text while the cursor is anywhere in it.
    const auto refresh = [this](int number) {
        const QTextBlock line = document()->findBlockByNumber(number);
        auto [first, last] = diagramFence(line);
        if (first < 0)
            std::tie(first, last) = mathFence(line);
        if (first < 0) {
            rehighlightBlock(line);
            return;
        }
        for (int i = first; i <= last; ++i)
            rehighlightBlock(document()->findBlockByNumber(i));
    };
    if (before >= 0)
        refresh(before);
    refresh(block);
    refreshFocus();
}

void MarkdownHighlighter::refreshFocus()
{
    // Out of the editor nothing is faded, as nothing is being written.
    const std::pair<int, int> paragraph =
        m_focus && m_cursorBlock >= 0 ? paragraphOf(m_cursorBlock) : std::pair{-1, -1};
    if (paragraph == m_paragraph)
        return;
    const std::pair<int, int> was = std::exchange(m_paragraph, paragraph);
    if (was.first < 0 || paragraph.first < 0) {
        rehighlight();
        return;
    }
    rehighlightRange(was);
    rehighlightRange(paragraph);
}

std::pair<int, int> MarkdownHighlighter::paragraphOf(int block) const
{
    const auto blank = [this](int number) {
        const QTextBlock line = document()->findBlockByNumber(number);
        return !line.isValid() || line.text().trimmed().isEmpty();
    };
    if (block < 0 || blank(block))
        return {block, block};
    int first = block;
    int last = block;
    while (first > 0 && !blank(first - 1))
        --first;
    while (last + 1 < document()->blockCount() && !blank(last + 1))
        ++last;
    return {first, last};
}

void MarkdownHighlighter::rehighlightRange(std::pair<int, int> range)
{
    for (int i = std::max(range.first, 0); i <= range.second; ++i)
        rehighlightBlock(document()->findBlockByNumber(i));
}

void MarkdownHighlighter::setSpeller(const Sonnet::Speller *speller)
{
    m_speller = speller;
    rehighlight();
}

namespace {

const QRegularExpression &wordPattern()
{
    static const QRegularExpression word(uR"([\p{L}][\p{L}\p{M}'\x{2019}]*)"_s);
    return word;
}

} // namespace

std::pair<int, int> MarkdownHighlighter::misspelledAt(int position) const
{
    const QTextBlock block = document() ? document()->findBlock(position) : QTextBlock();
    if (!block.isValid() || !block.layout())
        return {-1, -1};
    const int at = position - block.position();
    for (const QTextLayout::FormatRange &range : block.layout()->formats()) {
        if (range.format.underlineStyle() != QTextCharFormat::SpellCheckUnderline ||
            at < range.start || at > range.start + range.length)
            continue;
        for (const QRegularExpressionMatch &m : wordPattern().globalMatch(block.text())) {
            if (at < m.capturedStart() || at > m.capturedEnd())
                continue;
            // The word as checked and underlined, without a closing quote.
            qsizetype end = m.capturedEnd();
            while (end > m.capturedStart() &&
                   (block.text().at(end - 1) == u'\'' || block.text().at(end - 1) == QChar(0x2019)))
                --end;
            return {block.position() + int(m.capturedStart()), block.position() + int(end)};
        }
    }
    return {-1, -1};
}

void MarkdownHighlighter::setFoldComments(bool fold)
{
    if (m_foldComments == fold)
        return;
    m_foldComments = fold;
    rehighlight();
}

void MarkdownHighlighter::setFlash(int start, int end)
{
    QTextDocument *doc = document();
    if (!doc)
        return;
    const QTextCursor oldStart = m_flashStart;
    const QTextCursor oldEnd = m_flashEnd;
    m_flashStart = {};
    m_flashEnd = {};
    if (end > start) {
        const int last = doc->characterCount() - 1;
        m_flashStart = QTextCursor(doc);
        m_flashStart.setPosition(std::clamp(start, 0, last));
        m_flashEnd = QTextCursor(doc);
        m_flashEnd.setPosition(std::clamp(end, 0, last));
    }
    // The lines painted before, wherever edits since have moved them, and those now.
    for (const auto &[from, to] :
         {std::pair{oldStart, oldEnd}, std::pair{m_flashStart, m_flashEnd}}) {
        if (!from.isNull())
            rehighlightRange({from.block().blockNumber(), to.block().blockNumber()});
    }
}

void MarkdownHighlighter::setMatches(const QList<int> &positions)
{
    QTextDocument *doc = document();
    if (!doc)
        return;
    QList<int> sorted = positions;
    std::ranges::sort(sorted);
    if (sorted == m_matches)
        return;
    // The lines with matches before or now, drawn again, each once.
    QList<int> blocks;
    for (const QList<int> &list : {m_matches, sorted}) {
        for (const int position : list)
            blocks.append(doc->findBlock(position).blockNumber());
    }
    m_matches = sorted;
    std::ranges::sort(blocks);
    blocks.erase(std::unique(blocks.begin(), blocks.end()), blocks.end());
    for (const int block : std::as_const(blocks))
        rehighlightBlock(doc->findBlockByNumber(block));
}

void MarkdownHighlighter::setFound(const QList<int> &starts, int length, int current)
{
    QTextDocument *doc = document();
    if (!doc)
        return;
    QList<int> sorted = starts;
    std::ranges::sort(sorted);
    if (sorted == m_found && length == m_foundLength && current == m_foundCurrent)
        return;
    QList<int> blocks;
    for (const auto &[list, size] :
         {std::pair{m_found, m_foundLength}, std::pair{sorted, length}}) {
        for (const int start : list) {
            const int first = doc->findBlock(start).blockNumber();
            const int last = doc->findBlock(start + std::max(size - 1, 0)).blockNumber();
            for (int block = first; block <= last; ++block)
                blocks.append(block);
        }
    }
    m_found = sorted;
    m_foundLength = length;
    m_foundCurrent = current;
    std::ranges::sort(blocks);
    blocks.erase(std::unique(blocks.begin(), blocks.end()), blocks.end());
    for (const int block : std::as_const(blocks))
        rehighlightBlock(doc->findBlockByNumber(block));
}

void MarkdownHighlighter::setFocusMode(bool focus)
{
    if (m_focus == focus)
        return;
    m_focus = focus;
    m_paragraph =
        focus && document() && m_cursorBlock >= 0 ? paragraphOf(m_cursorBlock) : std::pair{-1, -1};
    rehighlight();
}

void MarkdownHighlighter::setSourceMode(bool source)
{
    if (m_source == source)
        return;
    m_source = source;
    rehighlight();
}

void MarkdownHighlighter::merge(int start, int length, const QTextCharFormat &format)
{
    for (int i = start; i < start + length; ++i) {
        QTextCharFormat current = QSyntaxHighlighter::format(i);
        current.merge(format);
        setFormat(i, 1, current);
    }
}

void MarkdownHighlighter::mark(int start, int length)
{
    QTextCharFormat format;
    if (m_cursorHere) {
        format.setForeground(m_look.faint);
    } else {
        // Folded away: there to edit, but taking next to no room.
        format.setForeground(Qt::transparent);
        format.setProperty(QTextFormat::FontPixelSize, 1);
    }
    merge(start, length, format);
}

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    styleBlock(text);
    // A yank's flash, over whatever the line is styled as.
    if (!m_flashStart.isNull() && m_flashEnd.position() > m_flashStart.position()) {
        const int at = currentBlock().position();
        const int start = std::max(m_flashStart.position() - at, 0);
        const int end = std::min(m_flashEnd.position() - at, int(text.size()));
        if (end > start) {
            QTextCharFormat flash;
            QColor tint = m_look.accent;
            tint.setAlphaF(0.4f);
            flash.setBackground(tint);
            merge(start, end - start, flash);
        }
    }
    if (!m_matches.isEmpty()) {
        const int at = currentBlock().position();
        QTextCharFormat match;
        QColor tint = m_look.accent;
        tint.setAlphaF(0.3f);
        match.setBackground(tint);
        match.setForeground(m_look.accent);
        for (auto it = std::ranges::lower_bound(m_matches, at);
             it != m_matches.cend() && *it < at + text.size(); ++it)
            merge(*it - at, 1, match);
    }
    if (!m_found.isEmpty() && m_foundLength > 0) {
        const int at = currentBlock().position();
        const int end = at + int(text.size());
        // From the first match that could still reach into this line.
        for (auto it = std::ranges::lower_bound(m_found, at - m_foundLength + 1);
             it != m_found.cend() && *it < end; ++it) {
            const int from = std::max(*it, at);
            const int to = std::min(*it + m_foundLength, end);
            if (to <= from)
                continue;
            QTextCharFormat found;
            QColor tint = m_look.accent;
            const bool current = it - m_found.begin() == m_foundCurrent;
            tint.setAlphaF(current ? 0.6f : 0.25f);
            found.setBackground(tint);
            merge(from - at, to - from, found);
        }
    }
    const int number = currentBlock().blockNumber();
    if (!m_focus || m_paragraph.first < 0 ||
        (number >= m_paragraph.first && number <= m_paragraph.second))
        return;
    // Faded, apart from what is folded away and drawn over.
    for (int i = 0; i < text.size(); ++i) {
        QTextCharFormat current = format(i);
        if (current.foreground().color().alpha() == 0)
            continue;
        current.setForeground(m_look.faint);
        setFormat(i, 1, current);
    }
}

void MarkdownHighlighter::styleBlock(const QString &text)
{
    const int state = kindOf(previousBlockState());
    const bool first = currentBlock().blockNumber() == 0;
    m_cursorHere = currentBlock().blockNumber() == m_cursorBlock;
    setCurrentBlockState(Normal);
    setCurrentBlockUserData(nullptr);

    if (m_source) {
        sourceBlock(text, state, first);
        return;
    }

    QTextCharFormat faint;
    faint.setForeground(m_look.faint);
    QTextCharFormat code;
    code.setFontFamilies({m_look.monoFamily.isEmpty() ? u"monospace"_s : m_look.monoFamily});
    code.setBackground(m_look.codeBackground);

    // Front matter: the user's YAML, shown quietly and never styled as Markdown.
    const QString trimmed = text.trimmed();
    if (first && (text == u"---" || text == u"﻿---")) {
        setFormat(0, int(text.size()), faint);
        setCurrentBlockState(InFrontMatter);
        return;
    }
    if (state == InFrontMatter) {
        QTextCharFormat yaml = faint;
        yaml.setProperty(QTextFormat::FontPixelSize, std::max(m_look.pixelSize - 2, 8));
        setFormat(0, int(text.size()), yaml);
        if (trimmed != u"---" && trimmed != u"...")
            setCurrentBlockState(InFrontMatter);
        return;
    }

    const QRegularExpressionMatch fence = fencePattern().match(text);
    if (state == InFence || fence.hasMatch()) {
        if (const auto [first, last] = diagramFence(currentBlock());
            first >= 0 && (m_cursorBlock < first || m_cursorBlock > last)) {
            const int number = currentBlock().blockNumber();
            if (number == first && diagramLine(text, last)) {
                setCurrentBlockState(fenceState(fence.captured(2), {}));
                return;
            }
            // The rest of a drawn diagram's fence folds away under it.
            if (number != first &&
                dynamic_cast<const Image *>(document()->findBlockByNumber(first).userData())) {
                mark(0, int(text.size()));
                if (number != last)
                    setCurrentBlockState(fenceState(QString(), {}));
                return;
            }
        }
        merge(0, int(text.size()), code);
        if (fence.hasMatch()) {
            merge(0, int(text.size()), faint);
            // An opening fence names the language the lines under it are read in.
            if (state != InFence) {
                auto *line = new CodeHighlighter::Line;
                line->language = fence.captured(2);
                setCurrentBlockState(fenceState(line->language, line->state));
                setCurrentBlockUserData(line);
            }
            return;
        }
        const auto *above =
            dynamic_cast<const CodeHighlighter::Line *>(currentBlock().previous().userData());
        auto *line = new CodeHighlighter::Line;
        if (above) {
            line->language = above->language;
            line->state = above->state;
        }
        for (const CodeHighlighter::Span &span :
             m_code->highlight(text, line->language, line->state))
            merge(span.start, span.length, span.format);
        setCurrentBlockState(fenceState(line->language, line->state));
        setCurrentBlockUserData(line);
        return;
    }

    if (mathLine(text, state) || imageLine(text) || contentsLine(text))
        return;

    static const QRegularExpression heading(uR"(^(#{1,6})(\s+|$))"_s);
    static const QRegularExpression quote(uR"(^(\s*>\s?)+)"_s);
    static const QRegularExpression rule(uR"(^\s*([-*_])(\s*\1){2,}\s*$)"_s);
    int from = 0;
    if (const QRegularExpressionMatch m = heading.match(text); m.hasMatch()) {
        const int level = int(m.capturedLength(1));
        QTextCharFormat format;
        format.setProperty(QTextFormat::FontPixelSize,
                           int(m_look.pixelSize * kHeadingScale[level - 1] + 0.5));
        format.setFontWeight(m_look.boldWeight);
        if (level <= 2 && !m_look.displayFamily.isEmpty())
            format.setFontFamilies({m_look.displayFamily});
        merge(0, int(text.size()), format);
        mark(0, int(m.capturedLength()));
        from = int(m.capturedLength());
    } else if (rule.match(text).hasMatch()) {
        setFormat(0, int(text.size()), faint);
        return;
    } else if (trimmed.startsWith(u'|')) {
        // Table rows are monospace so their padded columns line up, and keep their marks,
        // since folding them away would pull the pipes out of line.
        m_cursorHere = true;
        QTextCharFormat mono;
        mono.setFontFamilies({m_look.monoFamily.isEmpty() ? u"monospace"_s : m_look.monoFamily});
        merge(0, int(text.size()), mono);
        for (qsizetype i = text.indexOf(u'|'); i >= 0; i = text.indexOf(u'|', i + 1))
            merge(int(i), 1, faint);
        static const QRegularExpression delimiter(uR"(^\s*\|[\s:|-]*$)"_s);
        if (delimiter.match(text).hasMatch()) {
            merge(0, int(text.size()), faint);
            return;
        }
    } else if (const QRegularExpressionMatch m = quote.match(text); m.hasMatch()) {
        QTextCharFormat format;
        format.setForeground(m_look.muted);
        format.setFontItalic(true);
        merge(0, int(text.size()), format);
        QTextCharFormat bar;
        bar.setForeground(m_look.accent);
        bar.setFontWeight(m_look.boldWeight);
        merge(0, int(m.capturedLength()), bar);
        from = int(m.capturedLength());
    } else if (const QRegularExpressionMatch m = listPattern().match(text); m.hasMatch()) {
        QTextCharFormat bullet;
        bullet.setForeground(m_look.accent);
        bullet.setFontWeight(m_look.boldWeight);
        merge(int(m.capturedStart(2)), int(m.capturedLength(2)), bullet);
        from = int(m.capturedEnd(3));
        if (m.capturedLength(4) > 0) {
            merge(int(m.capturedStart(4)), int(m.capturedEnd(6) - m.capturedStart(4)), bullet);
            from = int(m.capturedEnd());
            if (m.captured(5) != u" ") {
                QTextCharFormat done;
                done.setForeground(m_look.faint);
                done.setFontStrikeOut(true);
                merge(from, int(text.size()) - from, done);
            }
        }
    }
    inlineStyles(text, from);
    taskMarkerStyles(text);
}

void MarkdownHighlighter::sourceBlock(const QString &text, int state, bool first)
{
    // Every mark shows, faded, and nothing changes size, so the text lines up as written.
    m_cursorHere = true;
    QTextCharFormat mono;
    mono.setFontFamilies({m_look.monoFamily.isEmpty() ? u"monospace"_s : m_look.monoFamily});
    setFormat(0, int(text.size()), mono);
    QTextCharFormat faint;
    faint.setForeground(m_look.faint);
    QTextCharFormat accent;
    accent.setForeground(m_look.accent);
    accent.setFontWeight(m_look.boldWeight);

    const QString trimmed = text.trimmed();
    if (first && (text == u"---" || text == u"\ufeff---")) {
        merge(0, int(text.size()), faint);
        setCurrentBlockState(InFrontMatter);
        return;
    }
    if (state == InFrontMatter) {
        merge(0, int(text.size()), faint);
        if (trimmed != u"---" && trimmed != u"...")
            setCurrentBlockState(InFrontMatter);
        return;
    }

    const QRegularExpressionMatch fence = fencePattern().match(text);
    if (state == InFence || fence.hasMatch()) {
        if (fence.hasMatch()) {
            merge(0, int(text.size()), faint);
            if (state != InFence) {
                auto *line = new CodeHighlighter::Line;
                line->language = fence.captured(2);
                setCurrentBlockState(fenceState(line->language, line->state));
                setCurrentBlockUserData(line);
            }
            return;
        }
        const auto *above =
            dynamic_cast<const CodeHighlighter::Line *>(currentBlock().previous().userData());
        auto *line = new CodeHighlighter::Line;
        if (above) {
            line->language = above->language;
            line->state = above->state;
        }
        for (const CodeHighlighter::Span &span :
             m_code->highlight(text, line->language, line->state))
            merge(span.start, span.length, span.format);
        setCurrentBlockState(fenceState(line->language, line->state));
        setCurrentBlockUserData(line);
        return;
    }

    static const QRegularExpression heading(uR"(^(#{1,6})(\s+|$))"_s);
    static const QRegularExpression quote(uR"(^(\s*>\s?)+)"_s);
    static const QRegularExpression rule(uR"(^\s*([-*_])(\s*\1){2,}\s*$)"_s);
    int from = 0;
    if (const QRegularExpressionMatch m = heading.match(text); m.hasMatch()) {
        QTextCharFormat bold;
        bold.setFontWeight(m_look.boldWeight);
        merge(0, int(text.size()), bold);
        merge(0, int(m.capturedLength(1)), accent);
        from = int(m.capturedLength());
    } else if (rule.match(text).hasMatch()) {
        merge(0, int(text.size()), faint);
        return;
    } else if (trimmed.startsWith(u'|')) {
        for (qsizetype i = text.indexOf(u'|'); i >= 0; i = text.indexOf(u'|', i + 1))
            merge(int(i), 1, faint);
        static const QRegularExpression delimiter(uR"(^\s*\|[\s:|-]*$)"_s);
        if (delimiter.match(text).hasMatch()) {
            merge(0, int(text.size()), faint);
            return;
        }
    } else if (const QRegularExpressionMatch m = quote.match(text); m.hasMatch()) {
        QTextCharFormat quoted;
        quoted.setForeground(m_look.muted);
        quoted.setFontItalic(true);
        merge(0, int(text.size()), quoted);
        merge(0, int(m.capturedLength()), accent);
        from = int(m.capturedLength());
    } else if (const QRegularExpressionMatch m = listPattern().match(text); m.hasMatch()) {
        merge(int(m.capturedStart(2)), int(m.capturedLength(2)), accent);
        from = int(m.capturedEnd(3));
        if (m.capturedLength(4) > 0) {
            merge(int(m.capturedStart(4)), int(m.capturedEnd(6) - m.capturedStart(4)), accent);
            from = int(m.capturedEnd());
            if (m.captured(5) != u" ") {
                QTextCharFormat done = faint;
                done.setFontStrikeOut(true);
                merge(from, int(text.size()) - from, done);
            }
        }
    }
    inlineStyles(text, from);
    taskMarkerStyles(text);
}

void MarkdownHighlighter::taskMarkerStyles(const QString &text)
{
    const QRegularExpressionMatch m = listPattern().match(text);
    // Only an open to-do's: a done one is all faded.
    if (!m.hasMatch() || m.capturedLength(4) == 0 || m.captured(5) != u" ")
        return;
    const int from = int(m.capturedEnd());
    // Each kind in a color of its own from the theme's code colors, none the accent tags are
    // in, so they tell apart at a glance in any theme.
    for (const TaskMarker &marker : taskMarkers(QStringView(text).sliced(from))) {
        QTextCharFormat marked;
        marked.setForeground(marker.kind == TaskMarker::Due        ? m_look.code.number
                             : marker.kind == TaskMarker::Repeat   ? m_look.code.type
                             : marker.kind == TaskMarker::Priority ? m_look.danger
                                                                   : m_look.code.string);
        marked.setFontWeight(m_look.strongWeight);
        merge(from + int(marker.start), int(marker.end - marker.start), marked);
    }
}

QList<std::pair<int, int>> MarkdownHighlighter::criticStyles(const QString &text, int from)
{
    QList<std::pair<int, int>> comments;
    if (!text.contains(u'{'))
        return comments;
    // Code spans keep their marks as written, so they are blanked before matching.
    static const QRegularExpression span(uR"((`+)(.+?)\1)"_s);
    QString prose = text;
    for (const QRegularExpressionMatch &m : span.globalMatch(text))
        prose.replace(m.capturedStart(), m.capturedLength(), QString(m.capturedLength(), u' '));
    QTextCharFormat added;
    added.setForeground(m_look.accent);
    added.setFontUnderline(true);
    QTextCharFormat removed;
    removed.setForeground(m_look.danger);
    removed.setFontStrikeOut(true);
    QTextCharFormat highlighted;
    QColor tint = m_look.accent;
    tint.setAlphaF(0.25f);
    highlighted.setBackground(tint);
    // Marks of three characters each side: {++ and ++}, and the like.
    const auto styled = [&](const QRegularExpression &pattern, const QTextCharFormat &format) {
        for (const QRegularExpressionMatch &m : pattern.globalMatch(prose, from)) {
            mark(int(m.capturedStart()), 3);
            merge(int(m.capturedStart(1)), int(m.capturedLength(1)), format);
            mark(int(m.capturedEnd(1)), 3);
        }
    };
    static const QRegularExpression addition(uR"(\{\+\+(.+?)\+\+\})"_s);
    static const QRegularExpression deletion(uR"(\{--(.+?)--\})"_s);
    static const QRegularExpression highlight(uR"(\{==(.+?)==\})"_s);
    styled(addition, added);
    styled(deletion, removed);
    styled(highlight, highlighted);
    static const QRegularExpression substitution(uR"(\{~~(.*?)~>(.*?)~~\})"_s);
    for (const QRegularExpressionMatch &m : substitution.globalMatch(prose, from)) {
        mark(int(m.capturedStart()), 3);
        merge(int(m.capturedStart(1)), int(m.capturedLength(1)), removed);
        mark(int(m.capturedEnd(1)), 2);
        merge(int(m.capturedStart(2)), int(m.capturedLength(2)), added);
        mark(int(m.capturedEnd(2)), 3);
    }

    static const QRegularExpression comment(uR"(\{>>(.*?)<<\})"_s);
    QList<std::pair<int, QString>> found;
    for (const QRegularExpressionMatch &m : comment.globalMatch(prose, from)) {
        const int start = int(m.capturedStart());
        const int length = int(m.capturedLength());
        comments.append({start, start + length});
        // The words as written, code spans and all.
        found.append({start, text.mid(m.capturedStart(1), m.capturedLength(1)).trimmed()});
        if (m_foldComments && !m_cursorHere) {
            // Away to the margin, which shows it.
            mark(start, length);
            continue;
        }
        QTextCharFormat aside;
        aside.setForeground(m_look.muted);
        aside.setFontItalic(true);
        aside.setBackground(tint);
        mark(start, 3);
        merge(int(m.capturedStart(1)), int(m.capturedLength(1)), aside);
        mark(int(m.capturedEnd(1)), 3);
    }
    // Source mode leaves comments in the text, out of the margin.
    if (Inlines *data = found.isEmpty() || m_source ? nullptr : inlines())
        data->comments = found;
    return comments;
}

MarkdownHighlighter::Inlines *MarkdownHighlighter::inlines()
{
    if (auto *data = dynamic_cast<Inlines *>(currentBlockUserData()))
        return data;
    if (currentBlockUserData())
        return nullptr;
    auto *data = new Inlines;
    setCurrentBlockUserData(data);
    return data;
}

QList<std::pair<int, int>> MarkdownHighlighter::inlineMathStyles(const QString &text, int from)
{
    QList<std::pair<int, int>> spans;
    for (const auto &[start, end] : inlineMath(text)) {
        if (start < from)
            continue;
        spans.append({int(start), int(end)});
        const QString latex = text.mid(start + 1, end - start - 2);
        const MathRenderer::Drawn drawn =
            m_cursorHere || m_source
                ? MathRenderer::Drawn()
                : MathRenderer::instance().draw(latex, false, m_look.pixelSize, m_look.text);
        Inlines *data = drawn.ok() ? inlines() : nullptr;
        if (!data) {
            // Being written, or not drawn: the TeX as written, in the code font.
            QTextCharFormat tex;
            tex.setFontFamilies({m_look.monoFamily.isEmpty() ? u"monospace"_s : m_look.monoFamily});
            merge(int(start), int(end - start), tex);
            QTextCharFormat faint;
            faint.setForeground(m_look.faint);
            merge(int(start), 1, faint);
            merge(int(end) - 1, 1, faint);
            continue;
        }
        data->math.append({int(start), drawn.path, drawn.size});
        // The first dollar sign takes the drawing's room, the rest folds away.
        QFont font;
        if (!m_look.family.isEmpty())
            font.setFamily(m_look.family);
        font.setPixelSize(std::max(drawn.size.height(), 1));
        const double lineHeight = QFontMetricsF(font).height();
        const int pixelSize =
            std::max(m_look.pixelSize, int(drawn.size.height() * drawn.size.height() / lineHeight));
        font.setPixelSize(pixelSize);
        QTextCharFormat room;
        room.setForeground(Qt::transparent);
        room.setProperty(QTextFormat::FontPixelSize, pixelSize);
        room.setFontLetterSpacingType(QFont::AbsoluteSpacing);
        room.setFontLetterSpacing(drawn.size.width() - QFontMetricsF(font).horizontalAdvance(u'$'));
        setFormat(int(start), 1, room);
        mark(int(start) + 1, int(end - start) - 1);
    }
    return spans;
}

std::pair<int, int> MarkdownHighlighter::mathFence(const QTextBlock &block) const
{
    QTextBlock open = block;
    if (!isMathFence(block.text()) && kindOf(block.previous().userState()) != InMath)
        return {-1, -1};
    // A line inside, or the closing line, finds its opening above.
    while (open.previous().isValid() && kindOf(open.previous().userState()) == InMath)
        open = open.previous();
    QString single;
    if (isMathFence(open.text(), &single) && !single.isEmpty())
        return {open.blockNumber(), open.blockNumber()};
    for (QTextBlock line = open.next(); line.isValid(); line = line.next()) {
        if (isMathFence(line.text()))
            return block.blockNumber() > line.blockNumber()
                       ? std::pair{-1, -1}
                       : std::pair{open.blockNumber(), line.blockNumber()};
    }
    return {-1, -1};
}

bool MarkdownHighlighter::mathLine(const QString &text, int state)
{
    QString single;
    const bool fence = isMathFence(text, &single);
    if (state != InMath && !fence)
        return false;
    const int number = currentBlock().blockNumber();
    // Inside, or opening, the lines after are math until a closing $$.
    const bool opening = state != InMath && fence && single.isEmpty();
    if (opening || (state == InMath && !fence))
        setCurrentBlockState(InMath);
    const auto [first, last] = mathFence(currentBlock());
    const bool away = first >= 0 && !m_source && (m_cursorBlock < first || m_cursorBlock > last);
    if (away && number == first) {
        QString latex = single;
        if (latex.isEmpty()) {
            QStringList lines;
            for (QTextBlock line = currentBlock().next();
                 line.isValid() && line.blockNumber() < last; line = line.next())
                lines.append(line.text());
            latex = lines.join(u'\n');
        }
        const MathRenderer::Drawn drawn =
            MathRenderer::instance().draw(latex, true, m_look.pixelSize, m_look.text);
        if (drawn.ok()) {
            QSize shown = drawn.size;
            if (shown.width() > m_look.imageWidth)
                shown = QSize(m_look.imageWidth,
                              drawn.size.height() * m_look.imageWidth / drawn.size.width());
            showImage(text, int(text.indexOf(u'$')), drawn.path, shown);
            return true;
        }
    }
    // The rest of drawn math folds away under it.
    if (away && number != first &&
        dynamic_cast<const Image *>(document()->findBlockByNumber(first).userData())) {
        mark(0, int(text.size()));
        return true;
    }
    QTextCharFormat tex;
    tex.setFontFamilies({m_look.monoFamily.isEmpty() ? u"monospace"_s : m_look.monoFamily});
    merge(0, int(text.size()), tex);
    if (fence) {
        QTextCharFormat faint;
        faint.setForeground(m_look.faint);
        merge(0, int(text.size()), faint);
    }
    return true;
}

void MarkdownHighlighter::inlineStyles(const QString &text, int from)
{
    QList<Range> taken;
    const auto free = [&](const QRegularExpressionMatch &m) {
        return m.capturedStart() >= from &&
               !overlaps(taken, int(m.capturedStart()), int(m.capturedEnd()));
    };
    const auto take = [&](const QRegularExpressionMatch &m) {
        taken.append({int(m.capturedStart()), int(m.capturedEnd())});
    };

    static const QRegularExpression span(uR"((`+)(.+?)\1)"_s);
    for (const QRegularExpressionMatch &m : span.globalMatch(text, from)) {
        if (!free(m))
            continue;
        QTextCharFormat code;
        code.setFontFamilies({m_look.monoFamily.isEmpty() ? u"monospace"_s : m_look.monoFamily});
        code.setBackground(m_look.codeBackground);
        merge(int(m.capturedStart(2)), int(m.capturedLength(2)), code);
        mark(int(m.capturedStart(1)), int(m.capturedLength(1)));
        mark(int(m.capturedEnd(2)), int(m.capturedLength(1)));
        take(m);
    }
    for (const auto &[start, end] : inlineMathStyles(text, from)) {
        if (!overlaps(taken, start, end))
            taken.append({start, end});
    }
    for (const auto &[start, end] : criticStyles(text, from)) {
        if (!overlaps(taken, start, end))
            taken.append({start, end});
    }

    QTextCharFormat link;
    link.setForeground(m_look.accent);
    link.setFontUnderline(true);
    for (const QRegularExpressionMatch &m : linkPattern().globalMatch(text, from)) {
        if (!free(m))
            continue;
        mark(int(m.capturedStart()), int(m.capturedStart(2) - m.capturedStart()));
        merge(int(m.capturedStart(2)), int(m.capturedLength(2)), link);
        mark(int(m.capturedEnd(2)), int(m.capturedEnd() - m.capturedEnd(2)));
        take(m);
    }
    for (const QRegularExpressionMatch &m : urlPattern().globalMatch(text, from)) {
        if (!free(m))
            continue;
        merge(int(m.capturedStart(1)), int(m.capturedLength(1)), link);
        take(m);
    }

    // Code, links and comments are not words to check; emphasized words are.
    QList<Range> unchecked = taken;

    const auto emphasis = [&](const QRegularExpression &pattern, int delimiter,
                              const QTextCharFormat &format) {
        for (const QRegularExpressionMatch &m : pattern.globalMatch(text, from)) {
            if (!free(m))
                continue;
            merge(int(m.capturedStart()), int(m.capturedLength()), format);
            mark(int(m.capturedStart()), delimiter);
            mark(int(m.capturedEnd()) - delimiter, delimiter);
            take(m);
        }
    };
    QTextCharFormat bold;
    bold.setFontWeight(m_look.boldWeight);
    QTextCharFormat italic;
    italic.setFontItalic(true);
    QTextCharFormat strike;
    strike.setFontStrikeOut(true);
    static const QRegularExpression strongStars(uR"(\*\*(?=\S)(.+?)(?<=\S)\*\*)"_s);
    static const QRegularExpression strongUnderscores(uR"((?<!\w)__(?=\S)(.+?)(?<=\S)__(?!\w))"_s);
    static const QRegularExpression struck(uR"(~~(?=\S)(.+?)(?<=\S)~~)"_s);
    static const QRegularExpression emStar(uR"((?<![*\w])\*(?=[^\s*])(.+?)(?<=[^\s*])\*(?!\*))"_s);
    static const QRegularExpression emUnderscore(uR"((?<![_\w])_(?=\S)(.+?)(?<=\S)_(?![_\w]))"_s);
    emphasis(strongStars, 2, bold);
    emphasis(strongUnderscores, 2, bold);
    emphasis(struck, 2, strike);
    emphasis(emStar, 1, italic);
    emphasis(emUnderscore, 1, italic);

    static const QRegularExpression tag(
        uR"((?:^|(?<=[\s(,]))#([\p{L}\p{N}_/-]*[\p{L}_/-][\p{L}\p{N}_/-]*))"_s);
    QTextCharFormat tagged;
    tagged.setForeground(m_look.accent);
    tagged.setFontWeight(m_look.strongWeight);
    for (const QRegularExpressionMatch &m : tag.globalMatch(text, from)) {
        if (free(m))
            merge(int(m.capturedStart()), int(m.capturedLength()), tagged);
        unchecked.append({int(m.capturedStart()), int(m.capturedEnd())});
    }

    if (!m_speller || m_source)
        return;
    QTextCharFormat misspelled;
    misspelled.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
    misspelled.setUnderlineColor(m_look.danger);
    for (const QRegularExpressionMatch &m : wordPattern().globalMatch(text, from)) {
        const int start = int(m.capturedStart());
        const int end = int(m.capturedEnd());
        // A word joined to digits, a slash or an @ is a name or a path, not prose.
        const auto joined = [&](int at) {
            return at >= 0 && at < text.size() &&
                   (text.at(at).isDigit() || text.at(at) == u'/' || text.at(at) == u'@' ||
                    text.at(at) == u'_');
        };
        if (overlaps(unchecked, start, end) || joined(start - 1) || joined(end))
            continue;
        QString word = m.captured();
        while (word.endsWith(u'\'') || word.endsWith(QChar(0x2019)))
            word.chop(1);
        if (m_speller->isMisspelled(word))
            merge(start, int(word.size()), misspelled);
    }
}

MarkdownHighlighter::Hit MarkdownHighlighter::hitAt(int position) const
{
    if (!document())
        return {};
    const QTextBlock block = document()->findBlock(position);
    if (!block.isValid() || kindOf(block.userState()) != Normal)
        return {};
    const QString text = block.text();
    const int at = position - block.position();
    const auto contains = [at](const QRegularExpressionMatch &m) {
        return at >= m.capturedStart() && at < m.capturedEnd();
    };

    if (const QRegularExpressionMatch m = listPattern().match(text);
        m.hasMatch() && m.capturedLength(4) > 0 && at >= m.capturedStart(4) &&
        at < m.capturedEnd(6))
        return {Hit::TaskBox, {}, block.position() + int(m.capturedStart(5))};
    for (const QRegularExpressionMatch &m : linkPattern().globalMatch(text)) {
        if (!contains(m))
            continue;
        QString target = m.captured(3);
        if (target.startsWith(u'<'))
            target = target.mid(1, target.size() - 2);
        static const QRegularExpression scheme(u"^[A-Za-z][A-Za-z0-9+.-]*:"_s);
        return {scheme.match(target).hasMatch() ? Hit::Url : Hit::Link, target, -1};
    }
    for (const QRegularExpressionMatch &m : urlPattern().globalMatch(text)) {
        if (contains(m))
            return {Hit::Url, m.captured(1), -1};
    }
    return {};
}

} // namespace jot
