#include "MarkdownHighlighter.h"

#include "CodeHighlighter.h"

#include "jot/Diagram.h"
#include "jot/FrontMatter.h"

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

const QRegularExpression &wikiPattern()
{
    static const QRegularExpression wiki(uR"((!?)\[\[([^\[\]|]*?)(?:\|([^\[\]]*))?\]\])"_s);
    return wiki;
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
        uR"(^(\s*)!(?:\[[^\]]*\]\(\s*(<[^>]*>|[^\s)]+)(?:\s+"[^"]*")?\s*\)|\[\[([^\]|#]+)(?:\|[^\]]*)?\]\])\s*$)"_s);
    const QRegularExpressionMatch m = line.match(text);
    if (!m.hasMatch() || !m_resolver)
        return false;
    const bool wiki = m.capturedLength(3) > 0;
    QString target = wiki ? m.captured(3).trimmed() : m.captured(2);
    if (target.startsWith(u'<'))
        target = target.mid(1, target.size() - 2);
    const QString key = (wiki ? u"w:"_s : u"m:"_s) + target;
    QString path = m_resolved.value(key);
    if (path.isEmpty()) {
        path = m_resolver(target, wiki);
        // Only what was found is kept, so a file added later is still found.
        if (!path.isEmpty())
            m_resolved.insert(key, path);
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

void MarkdownHighlighter::showImage(const QString &text, int bang, const QString &path, QSize shown)
{
    auto *image = new Image;
    image->path = path;
    image->size = shown;
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
    // A diagram shows as its text while the cursor is anywhere in its fence.
    const auto refresh = [this](int number) {
        const QTextBlock line = document()->findBlockByNumber(number);
        const auto [first, last] = diagramFence(line);
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

void MarkdownHighlighter::setFoldComments(bool fold)
{
    if (m_foldComments == fold)
        return;
    m_foldComments = fold;
    rehighlight();
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
        QTextCharFormat mono;
        mono.setFontFamilies({m_look.monoFamily.isEmpty() ? u"monospace"_s : m_look.monoFamily});
        setFormat(0, int(text.size()), mono);
        // Still knows code and front matter, so clicks there tick and follow nothing.
        const QString trimmed = text.trimmed();
        const bool fence = fencePattern().match(text).hasMatch();
        if (first && (text == u"---" || text == u"\ufeff---"))
            setCurrentBlockState(InFrontMatter);
        else if (state == InFrontMatter && trimmed != u"---" && trimmed != u"...")
            setCurrentBlockState(InFrontMatter);
        else if (state != InFrontMatter && (state == InFence ? !fence : fence))
            setCurrentBlockState(InFence);
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

    if (imageLine(text) || contentsLine(text))
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
}

QList<std::pair<int, int>> MarkdownHighlighter::criticStyles(const QString &text, int from)
{
    QList<std::pair<int, int>> comments;
    if (!text.contains(u'{'))
        return comments;
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
        for (const QRegularExpressionMatch &m : pattern.globalMatch(text, from)) {
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
    for (const QRegularExpressionMatch &m : substitution.globalMatch(text, from)) {
        mark(int(m.capturedStart()), 3);
        merge(int(m.capturedStart(1)), int(m.capturedLength(1)), removed);
        mark(int(m.capturedEnd(1)), 2);
        merge(int(m.capturedStart(2)), int(m.capturedLength(2)), added);
        mark(int(m.capturedEnd(2)), 3);
    }

    static const QRegularExpression comment(uR"(\{>>(.*?)<<\})"_s);
    QList<std::pair<int, QString>> found;
    for (const QRegularExpressionMatch &m : comment.globalMatch(text, from)) {
        const int start = int(m.capturedStart());
        const int length = int(m.capturedLength());
        comments.append({start, start + length});
        found.append({start, m.captured(1).trimmed()});
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
    if (!found.isEmpty() && !currentBlockUserData()) {
        auto *data = new Comments;
        data->comments = found;
        setCurrentBlockUserData(data);
    }
    return comments;
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
    for (const auto &[start, end] : criticStyles(text, from)) {
        if (!overlaps(taken, start, end))
            taken.append({start, end});
    }

    QTextCharFormat link;
    link.setForeground(m_look.accent);
    link.setFontUnderline(true);
    for (const QRegularExpressionMatch &m : wikiPattern().globalMatch(text, from)) {
        if (!free(m))
            continue;
        // The label when there is one, else the target, between folded brackets.
        const int shownStart =
            int(m.capturedLength(3) > 0 ? m.capturedStart(3) : m.capturedStart(2));
        const int shownEnd = int(m.capturedLength(3) > 0 ? m.capturedEnd(3) : m.capturedEnd(2));
        mark(int(m.capturedStart()), shownStart - int(m.capturedStart()));
        merge(shownStart, shownEnd - shownStart, link);
        mark(shownEnd, int(m.capturedEnd()) - shownEnd);
        take(m);
    }
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
    for (const QRegularExpressionMatch &m : wikiPattern().globalMatch(text)) {
        if (contains(m))
            return {Hit::Wikilink, m.captured(2).trimmed(), -1};
    }
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
