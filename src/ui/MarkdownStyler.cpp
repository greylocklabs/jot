#include "MarkdownStyler.h"

#include "jot/TaskLine.h"

#include "MarkdownHighlighter.h"
#include "ThemeController.h"
#include "VaultController.h"

#include "jot/FrontMatter.h"
#include "jot/MarkdownEdit.h"
#include "jot/TextFind.h"
#include "jot/WritingStats.h"

#include <Sonnet/Speller>

#include <QDateTime>
#include <QTextCursor>

#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

MarkdownStyler::MarkdownStyler(QObject *parent)
    : QObject(parent), m_widthSettle(new QTimer(this)), m_flashEnd(new QTimer(this))
{
    m_flashEnd->setSingleShot(true);
    m_flashEnd->setInterval(500);
    connect(m_flashEnd, &QTimer::timeout, this, [this] {
        if (m_highlighter)
            m_highlighter->setFlash(-1, -1);
    });
    m_widthSettle->setSingleShot(true);
    m_widthSettle->setInterval(150);
    connect(m_widthSettle, &QTimer::timeout, this, &MarkdownStyler::applyLook);
    connect(ThemeController::instance(), &ThemeController::changed, this,
            &MarkdownStyler::applyLook);
}

MarkdownStyler::~MarkdownStyler() = default;

void MarkdownStyler::setSpellcheck(bool on)
{
    if (m_spellcheck == on)
        return;
    m_spellcheck = on;
    applySpelling();
    Q_EMIT spellingChanged();
}

void MarkdownStyler::setLanguage(const QString &language)
{
    if (m_language == language)
        return;
    m_language = language;
    applySpelling();
    Q_EMIT spellingChanged();
}

void MarkdownStyler::applySpelling()
{
    if (m_spellcheck && !m_speller)
        m_speller = std::make_unique<Sonnet::Speller>();
    if (m_speller) {
        // A language with no dictionary here falls back to the desktop's.
        const QString wanted = QString(m_language).replace(u'-', u'_');
        m_speller->setLanguage(!wanted.isEmpty() && m_speller->availableLanguages().contains(wanted)
                                   ? wanted
                                   : m_speller->defaultLanguage());
    }
    if (m_highlighter)
        m_highlighter->setSpeller(
            m_spellcheck && m_speller && m_speller->isValid() ? m_speller.get() : nullptr);
}

QVariantMap MarkdownStyler::misspelledAt(int position) const
{
    if (!m_highlighter || !m_spellcheck || !m_speller)
        return {};
    const auto [start, end] = m_highlighter->misspelledAt(position);
    if (start < 0)
        return {};
    const QString word = m_document->textDocument()->toPlainText().mid(start, end - start);
    return {{u"start"_s, start},
            {u"end"_s, end},
            {u"word"_s, word},
            {u"suggestions"_s, m_speller->suggest(word).mid(0, 6)}};
}

void MarkdownStyler::acceptWord(const QString &word, bool forNow)
{
    if (!m_speller)
        return;
    if (forNow)
        m_speller->addToSession(word);
    else
        m_speller->addToPersonal(word);
    if (m_highlighter)
        m_highlighter->rehighlight();
}

void MarkdownStyler::setDocument(QQuickTextDocument *document)
{
    if (m_document == document)
        return;
    delete m_highlighter;
    m_highlighter = nullptr;
    m_document = document;
    if (document && document->textDocument()) {
        m_highlighter = new MarkdownHighlighter(document->textDocument());
        m_highlighter->setResolver([](const QString &target) {
            VaultController *vault = VaultController::instance();
            return vault ? vault->attachmentPath(target) : QString();
        });
        applyLook();
        m_highlighter->setSourceMode(m_sourceMode);
        m_highlighter->setCursorPosition(m_cursorPosition);
        m_highlighter->setFocusMode(m_focusMode);
        m_highlighter->setFoldComments(m_foldComments);
        applySpelling();
        connect(document->textDocument(), &QTextDocument::contentsChanged, this,
                &MarkdownStyler::scheduleImages);
        connect(document->textDocument(), &QTextDocument::contentsChanged, this,
                &MarkdownStyler::readFrontMatter);
    }
    Q_EMIT documentChanged();
    // At once rather than scheduled: the images' places belong to the old text, and asking
    // the new text for them, as the editor would before the update came, reaches past it.
    Q_EMIT imagesChanged();
    scheduleImages();
    readFrontMatter();
}

void MarkdownStyler::readFrontMatter()
{
    findClosingFence();
    QVariantMap keys;
    QVariantList problems;
    if (m_document && m_document->textDocument()) {
        const QString text = m_document->textDocument()->toPlainText();
        if (const NoteParts parts = splitFrontMatter(text); parts.hasFrontMatter)
            keys = jot::readFrontMatter(parts.frontMatter);
        for (const FrontMatterProblem &problem : jot::frontMatterProblems(text))
            problems.append(
                QVariantMap{{u"line"_s, problem.line}, {u"message"_s, problem.message}});
    }
    if (keys == m_frontMatter && problems == m_frontMatterProblems)
        return;
    m_frontMatter = keys;
    m_frontMatterProblems = problems;
    Q_EMIT frontMatterChanged();
}

void MarkdownStyler::flash(int start, int end)
{
    if (!m_highlighter)
        return;
    m_highlighter->setFlash(start, end);
    m_flashEnd->start();
}

void MarkdownStyler::setMatches(const QVariantList &positions)
{
    if (!m_highlighter)
        return;
    QList<int> at;
    at.reserve(positions.size());
    for (const QVariant &position : positions)
        at.append(position.toInt());
    m_highlighter->setMatches(at);
}

QVariantList MarkdownStyler::findAll(const QString &needle, bool matchCase, bool wholeWords) const
{
    QVariantList starts;
    if (!m_document || !m_document->textDocument())
        return starts;
    const QString text = m_document->textDocument()->toPlainText();
    for (const qsizetype start : jot::findAll(text, needle, matchCase, wholeWords))
        starts.append(int(start));
    return starts;
}

void MarkdownStyler::setFound(const QVariantList &starts, int length, int current)
{
    if (!m_highlighter)
        return;
    QList<int> at;
    at.reserve(starts.size());
    for (const QVariant &start : starts)
        at.append(start.toInt());
    m_highlighter->setFound(at, length, current);
}

int MarkdownStyler::replaceAll(const QVariantList &starts, int length, const QString &text)
{
    if (!m_document || !m_document->textDocument() || starts.isEmpty())
        return 0;
    QList<int> at;
    for (const QVariant &start : starts)
        at.append(start.toInt());
    std::ranges::sort(at, std::greater<>());
    QTextCursor cursor(m_document->textDocument());
    cursor.beginEditBlock();
    // From the end, so each start is still where it was found.
    for (const int start : std::as_const(at)) {
        cursor.setPosition(start);
        cursor.setPosition(start + length, QTextCursor::KeepAnchor);
        cursor.insertText(text);
    }
    cursor.endEditBlock();
    return int(at.size());
}

void MarkdownStyler::replace(int start, int end, const QString &text)
{
    if (!m_document || !m_document->textDocument())
        return;
    QTextCursor cursor(m_document->textDocument());
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    cursor.insertText(text);
}

void MarkdownStyler::findClosingFence()
{
    int found = -1;
    QString mark;
    if (m_document && m_document->textDocument()) {
        const QTextDocument *document = m_document->textDocument();
        const QTextBlock first = document->firstBlock();
        // Opened as the highlighter reads it: a first line of `---` alone, a byte order mark
        // before it aside.
        const bool opens = first.text() == u"---" || first.text() == u"\uFEFF---";
        for (QTextBlock block = first.next(); opens && block.isValid(); block = block.next()) {
            const QString line = block.text().trimmed();
            if (line != u"---" && line != u"...")
                continue;
            if (!block.contains(m_cursorPosition)) {
                found = block.position() + int(block.text().indexOf(line));
                mark = line;
            }
            break;
        }
    }
    if (found == m_closingFence && mark == m_closingFenceMark)
        return;
    m_closingFence = found;
    m_closingFenceMark = mark;
    Q_EMIT closingFenceChanged();
}

int MarkdownStyler::lineAt(int position) const
{
    if (!m_document || !m_document->textDocument())
        return -1;
    return m_document->textDocument()->findBlock(position).blockNumber();
}

int MarkdownStyler::lineStart(int line) const
{
    if (!m_document || !m_document->textDocument())
        return 0;
    const QTextDocument *text = m_document->textDocument();
    const QTextBlock block = text->findBlockByNumber(line);
    return block.isValid() ? block.position() : text->characterCount() - 1;
}

bool MarkdownStyler::tidyTaskLine(int line)
{
    QTextDocument *document = m_document ? m_document->textDocument() : nullptr;
    const QTextBlock block = document ? document->findBlockByNumber(line) : QTextBlock();
    // Code and front matter are kept as written, a to-do shown there as an example too.
    if (!block.isValid() ||
        MarkdownHighlighter::kindOf(block.userState()) != MarkdownHighlighter::Normal)
        return false;
    const std::optional<QString> tidied = tidiedTaskLine(block.text(), QDate::currentDate());
    if (!tidied)
        return false;
    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.insertText(*tidied);
    return true;
}

QVariantMap MarkdownStyler::linkTargetAt(int position) const
{
    if (!m_document || !m_document->textDocument())
        return {};
    const std::optional<markdownedit::LinkTargetQuery> query =
        markdownedit::linkTargetAt(m_document->textDocument()->toPlainText(), position);
    if (!query)
        return {};
    return {{u"start"_s, int(query->start)}, {u"text"_s, query->text}};
}

QVariantMap MarkdownStyler::emojiAt(int position) const
{
    if (!m_document || !m_document->textDocument())
        return {};
    const std::optional<markdownedit::EmojiQuery> query =
        markdownedit::emojiAt(m_document->textDocument()->toPlainText(), position);
    if (!query)
        return {};
    return {{u"start"_s, int(query->start)}, {u"text"_s, query->text}};
}

QVariantMap MarkdownStyler::markerAt(int position) const
{
    if (!m_document || !m_document->textDocument())
        return {};
    const std::optional<markdownedit::EmojiQuery> query =
        markdownedit::markerAt(m_document->textDocument()->toPlainText(), position);
    if (!query)
        return {};
    return {{u"start"_s, int(query->start)}, {u"text"_s, query->text}};
}

QVariantMap MarkdownStyler::frontMatterAt(int position) const
{
    if (!m_document || !m_document->textDocument())
        return {};
    const QTextDocument *document = m_document->textDocument();
    const QTextBlock first = document->firstBlock();
    if (first.text().trimmed() != u"---")
        return {};
    const QTextBlock here = document->findBlock(position);
    if (here.blockNumber() == 0)
        return {};
    // Within the fences only: a note that opens with a `---` rule and none after is all body.
    bool closed = false;
    for (QTextBlock block = first.next(); block.isValid(); block = block.next()) {
        const QString line = block.text().trimmed();
        if (line == u"---" || line == u"...") {
            if (block.blockNumber() <= here.blockNumber())
                return {};
            closed = true;
            break;
        }
    }
    if (!closed)
        return {};
    const QString before = here.text().left(position - here.position());
    // What was typed of a value ends at the cursor, so after a space nothing is being typed.
    const auto value = [&](const QString &key, QString typed) {
        while (!typed.isEmpty() && typed.front().isSpace())
            typed.remove(0, 1);
        if (!typed.isEmpty() && typed.back().isSpace())
            typed.clear();
        return QVariantMap{{u"kind"_s, u"value"_s},
                           {u"key"_s, key},
                           {u"text"_s, typed},
                           {u"start"_s, position - int(typed.size())}};
    };
    const qsizetype colon = before.indexOf(u':');
    if (colon < 0) {
        const QString trimmed = before.trimmed();
        // A list item: the value of the key above it.
        if (trimmed.startsWith(u'-') && (trimmed.size() == 1 || trimmed.at(1) == u' ')) {
            for (QTextBlock block = here.previous(); block.isValid() && block != first;
                 block = block.previous()) {
                const QString line = block.text();
                if (line.isEmpty() || line.front().isSpace() || line.startsWith(u'-'))
                    continue;
                const qsizetype at = line.indexOf(u':');
                if (at <= 0)
                    break;
                return value(line.left(at).trimmed(), before.mid(before.indexOf(u'-') + 1));
            }
            return {};
        }
        if (before.isEmpty() || before.front().isSpace() || before.contains(u' ') ||
            before.startsWith(u'#'))
            return {};
        return {{u"kind"_s, u"key"_s}, {u"text"_s, before}, {u"start"_s, int(here.position())}};
    }
    if (before.front().isSpace() || colon + 1 >= before.size() || before.at(colon + 1) != u' ')
        return {};
    const QString key = before.left(colon).trimmed();
    QString rest = before.mid(colon + 1);
    // In a list written on the line, the item after the last comma.
    if (rest.trimmed().startsWith(u'['))
        rest = rest.mid(qMax(rest.lastIndexOf(u','), rest.lastIndexOf(u'[')) + 1);
    return value(key, rest);
}

QStringList MarkdownStyler::languages() const
{
    // Spelling off has no speller to ask, so one is made for the question, once.
    if (m_speller)
        return m_speller->availableLanguages();
    static const QStringList installed = Sonnet::Speller().availableLanguages();
    return installed;
}

QVariantList MarkdownStyler::markerSuggestions(const QString &typed)
{
    QVariantList out;
    for (const MarkerSuggestion &each : jot::markerSuggestions(typed, QDate::currentDate()))
        out.append(QVariantMap{
            {u"text"_s, each.text}, {u"shown"_s, each.shown}, {u"detail"_s, each.detail}});
    return out;
}

QVariantMap MarkdownStyler::format(const QString &action, int from, int to, const QString &argument)
{
    using namespace markdownedit;
    if (!m_document || !m_document->textDocument())
        return {};
    QTextDocument *document = m_document->textDocument();
    const QString text = document->toPlainText();

    // Steps on the note as a whole run one after another, then land as one change.
    QString working = text;
    qsizetype start = from, end = to;
    const auto step = [&](const Edit &edit) {
        working.replace(edit.start, edit.end - edit.start, edit.text);
        start = edit.selectionStart;
        end = edit.selectionEnd;
    };
    static const QHash<QString, QString> markers{
        {u"bold"_s, u"**"_s}, {u"italic"_s, u"*"_s}, {u"strike"_s, u"~~"_s}, {u"code"_s, u"`"_s}};
    static const QHash<QString, LineStyle> styles{
        {u"bullet"_s, LineStyle::Bullet}, {u"numbered"_s, LineStyle::Numbered},
        {u"task"_s, LineStyle::Task},     {u"quote"_s, LineStyle::Quote},
        {u"h1"_s, LineStyle::Heading1},   {u"h2"_s, LineStyle::Heading2},
        {u"h3"_s, LineStyle::Heading3}};
    if (markers.contains(action)) {
        step(wrap(working, from, to, markers.value(action)));
    } else if (styles.contains(action)) {
        step(styleLines(working, from, to, styles.value(action)));
    } else if (action == u"newline" || action == u"indent" || action == u"outdent") {
        // A table takes the key first, then a list.
        const bool outdent = action == u"outdent";
        std::optional<Edit> edit =
            action == u"newline" ? newTableRow(working, from) : moveInTable(working, from, outdent);
        if (!edit)
            edit = action == u"newline" ? continueList(working, from, to)
                                        : indentList(working, from, to, outdent);
        if (!edit)
            return {};
        step(*edit);
    } else if (action == u"linkTarget") {
        const std::optional<Edit> edit = completeLinkTarget(
            working, from, argument.section(u'\n', 0, 0), argument.section(u'\n', 1));
        if (!edit)
            return {};
        step(*edit);
    } else if (action == u"table") {
        // In a table, however much of it is selected, the button lines it up.
        const std::optional<Edit> lined = formatTable(working, from, to);
        step(lined ? *lined : insertTable(working, from, to));
    } else if (action == u"link") {
        step(link(working, from, to));
    } else if (action == u"codeblock") {
        step(codeBlock(working, from, to));
    } else {
        return {};
    }

    const Edit edit = difference(text, working, start, end);
    if (edit.start != edit.end || !edit.text.isEmpty()) {
        QTextCursor cursor(document);
        cursor.beginEditBlock();
        cursor.setPosition(int(edit.start));
        cursor.setPosition(int(edit.end), QTextCursor::KeepAnchor);
        cursor.insertText(edit.text);
        cursor.endEditBlock();
    }
    return {{u"start"_s, int(start)}, {u"end"_s, int(end)}};
}

void MarkdownStyler::setSourceMode(bool source)
{
    if (m_sourceMode == source)
        return;
    m_sourceMode = source;
    if (m_highlighter)
        m_highlighter->setSourceMode(source);
    Q_EMIT sourceModeChanged();
    // Images come and go with the styling.
    scheduleImages();
}

void MarkdownStyler::setFoldComments(bool fold)
{
    if (m_foldComments == fold)
        return;
    m_foldComments = fold;
    if (m_highlighter)
        m_highlighter->setFoldComments(fold);
    Q_EMIT foldCommentsChanged();
    scheduleImages();
}

QVariantList MarkdownStyler::comments() const
{
    QVariantList comments;
    if (!m_document || !m_document->textDocument())
        return comments;
    for (QTextBlock block = m_document->textDocument()->begin(); block.isValid();
         block = block.next()) {
        const auto *found = dynamic_cast<const MarkdownHighlighter::Inlines *>(block.userData());
        if (!found)
            continue;
        for (const auto &[at, text] : found->comments)
            comments.append(QVariantMap{{u"position"_s, block.position() + at}, {u"text"_s, text}});
    }
    return comments;
}

void MarkdownStyler::setFocusMode(bool focus)
{
    if (m_focusMode == focus)
        return;
    m_focusMode = focus;
    if (m_highlighter)
        m_highlighter->setFocusMode(focus);
    Q_EMIT focusModeChanged();
}

void MarkdownStyler::setCursorPosition(int position)
{
    if (m_cursorPosition == position)
        return;
    m_cursorPosition = position;
    if (m_highlighter)
        m_highlighter->setCursorPosition(position);
    Q_EMIT cursorPositionChanged();
    findClosingFence();
    scheduleImages();
}

void MarkdownStyler::setFontFamily(const QString &family)
{
    if (m_fontFamily == family)
        return;
    m_fontFamily = family;
    applyLook();
    Q_EMIT fontFamilyChanged();
}

void MarkdownStyler::setPixelSize(int size)
{
    if (m_pixelSize == size)
        return;
    m_pixelSize = size;
    applyLook();
    Q_EMIT pixelSizeChanged();
}

void MarkdownStyler::setWidth(int width)
{
    if (m_width == width)
        return;
    m_width = width;
    m_widthSettle->start();
    Q_EMIT widthChanged();
}

void MarkdownStyler::scheduleImages()
{
    if (m_imagesPending)
        return;
    m_imagesPending = true;
    QTimer::singleShot(0, this, [this] {
        m_imagesPending = false;
        if (m_highlighter)
            m_highlighter->refreshFocus();
        // A heading changed elsewhere in the note redraws each table of contents.
        if (m_highlighter && m_document && m_document->textDocument()) {
            const QString text = m_document->textDocument()->toPlainText();
            QList<std::pair<int, QString>> headings;
            if (text.contains(u"${toc}"_s)) {
                for (const Heading &heading : headingsOf(splitFrontMatter(text).body))
                    headings.append({heading.level, heading.text});
            }
            if (headings != m_headings) {
                m_headings = headings;
                m_highlighter->refreshContents();
            }
        }
        Q_EMIT imagesChanged();
    });
}

int MarkdownStyler::headingAt(const QVariantList &headings, int position)
{
    int under = -1;
    for (int i = 0; i < headings.size(); ++i) {
        if (headings.at(i).toMap().value(u"position"_s).toInt() > position)
            break;
        under = i;
    }
    return under;
}

QVariantMap MarkdownStyler::countOf(const QString &text)
{
    const TextCount count = countText(text);
    return {{u"words"_s, count.words}, {u"characters"_s, count.characters}};
}

QVariantMap MarkdownStyler::count() const
{
    if (!m_document || !m_document->textDocument())
        return countOf({});
    const QString note = m_document->textDocument()->toPlainText();
    return countOf(splitFrontMatter(note).body.toString());
}

QVariantList MarkdownStyler::headings() const
{
    QVariantList headings;
    if (!m_document || !m_document->textDocument())
        return headings;
    const QString note = m_document->textDocument()->toPlainText();
    const QStringView body = splitFrontMatter(note).body;
    const QList<Heading> found = headingsOf(body);
    const int top = found.isEmpty() ? 1 : std::ranges::min(found, {}, &Heading::level).level;
    for (const Heading &heading : found)
        headings.append(
            QVariantMap{{u"text"_s, heading.text},
                        {u"level"_s, heading.level - top + 1},
                        {u"position"_s, int(heading.position + (body.data() - note.data()))}});
    return headings;
}

QVariantList MarkdownStyler::contents() const
{
    QVariantList contents;
    if (!m_document || !m_document->textDocument())
        return contents;
    QVariantList headings;
    bool read = false;
    for (QTextBlock block = m_document->textDocument()->begin(); block.isValid();
         block = block.next()) {
        const auto *toc = dynamic_cast<const MarkdownHighlighter::Toc *>(block.userData());
        if (!toc)
            continue;
        if (!read) {
            read = true;
            headings = this->headings();
        }
        contents.append(QVariantMap{{u"position"_s, block.position()},
                                    {u"rowHeight"_s, toc->rowHeight},
                                    {u"headings"_s, headings}});
    }
    return contents;
}

QVariantList MarkdownStyler::images() const
{
    QVariantList images;
    if (!m_document || !m_document->textDocument())
        return images;
    for (QTextBlock block = m_document->textDocument()->begin(); block.isValid();
         block = block.next()) {
        if (const auto *image = dynamic_cast<const MarkdownHighlighter::Image *>(block.userData()))
            images.append(QVariantMap{{u"position"_s, block.position()},
                                      {u"source"_s, QUrl::fromLocalFile(image->path)},
                                      {u"width"_s, image->size.width()},
                                      {u"height"_s, image->size.height()},
                                      {u"audio"_s, image->audio},
                                      {u"video"_s, image->video}});
        if (const auto *line =
                dynamic_cast<const MarkdownHighlighter::Inlines *>(block.userData())) {
            for (const MarkdownHighlighter::Math &math : line->math)
                images.append(QVariantMap{{u"position"_s, block.position() + math.at},
                                          {u"source"_s, QUrl::fromLocalFile(math.path)},
                                          {u"width"_s, math.size.width()},
                                          {u"height"_s, math.size.height()},
                                          {u"inline"_s, true}});
        }
    }
    return images;
}

void MarkdownStyler::applyLook()
{
    if (!m_highlighter)
        return;
    const ThemeController &theme = *ThemeController::instance();
    MarkdownHighlighter::Look look;
    look.text = theme.text();
    look.muted = theme.textMuted();
    look.faint = theme.textFaint();
    look.accent = theme.accent();
    look.danger = theme.danger();
    look.codeBackground = theme.surfaceAlt();
    look.code = codeColors(theme.spec());
    look.family = m_fontFamily.isEmpty() ? theme.fontFamily() : m_fontFamily;
    look.monoFamily = theme.monoFontFamily();
    look.displayFamily = theme.displayFontFamily();
    look.pixelSize = m_pixelSize;
    look.strongWeight = theme.weightStrong();
    look.boldWeight = theme.weightBold();
    look.imageWidth = m_width;
    look.imageHeight = theme.imageHeight();
    look.diagram = theme.diagramStyle(look.family, m_pixelSize);
    m_highlighter->setLook(look);
    scheduleImages();
}

QVariantMap MarkdownStyler::hitAt(int position) const
{
    if (!m_highlighter)
        return {};
    const MarkdownHighlighter::Hit hit = m_highlighter->hitAt(position);
    static const char *kinds[] = {"", "link", "url", "task"};
    return {{u"kind"_s, QLatin1String(kinds[hit.kind])},
            {u"target"_s, hit.target},
            {u"position"_s, hit.position}};
}

} // namespace jot
