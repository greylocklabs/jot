#include "MarkdownStyler.h"

#include "MarkdownHighlighter.h"
#include "ThemeController.h"
#include "VaultController.h"

#include "jot/FrontMatter.h"
#include "jot/MarkdownEdit.h"

#include <QDateTime>
#include <QTextCursor>

#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace jot {

MarkdownStyler::MarkdownStyler(QObject *parent) : QObject(parent), m_widthSettle(new QTimer(this))
{
    m_widthSettle->setSingleShot(true);
    m_widthSettle->setInterval(150);
    connect(m_widthSettle, &QTimer::timeout, this, &MarkdownStyler::applyLook);
    connect(ThemeController::instance(), &ThemeController::changed, this,
            &MarkdownStyler::applyLook);
}

MarkdownStyler::~MarkdownStyler() = default;

void MarkdownStyler::setDocument(QQuickTextDocument *document)
{
    if (m_document == document)
        return;
    delete m_highlighter;
    m_highlighter = nullptr;
    m_document = document;
    if (document && document->textDocument()) {
        m_highlighter = new MarkdownHighlighter(document->textDocument());
        m_highlighter->setResolver([](const QString &target, bool wiki) {
            VaultController *vault = VaultController::instance();
            return vault ? vault->attachmentPath(target, wiki) : QString();
        });
        applyLook();
        m_highlighter->setSourceMode(m_sourceMode);
        m_highlighter->setCursorPosition(m_cursorPosition);
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
    QVariantMap keys;
    if (m_document && m_document->textDocument()) {
        const QString text = m_document->textDocument()->toPlainText();
        if (const NoteParts parts = splitFrontMatter(text); parts.hasFrontMatter)
            keys = jot::readFrontMatter(parts.frontMatter);
    }
    if (keys == m_frontMatter)
        return;
    m_frontMatter = keys;
    Q_EMIT frontMatterChanged();
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

QVariantMap MarkdownStyler::wikilinkAt(int position) const
{
    if (!m_document || !m_document->textDocument())
        return {};
    const std::optional<markdownedit::WikilinkQuery> query =
        markdownedit::wikilinkAt(m_document->textDocument()->toPlainText(), position);
    if (!query)
        return {};
    return {{u"start"_s, int(query->start)}, {u"text"_s, query->text}};
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
    const auto setKey = [&](const QString &key, const QVariant &value) {
        step(markdownedit::setKey(working, start, end, key, value));
    };
    static const QHash<QString, QString> markers{
        {u"bold"_s, u"**"_s}, {u"italic"_s, u"*"_s}, {u"strike"_s, u"~~"_s}, {u"code"_s, u"`"_s}};
    static const QHash<QString, LineStyle> styles{
        {u"bullet"_s, LineStyle::Bullet}, {u"numbered"_s, LineStyle::Numbered},
        {u"task"_s, LineStyle::Task},     {u"quote"_s, LineStyle::Quote},
        {u"h1"_s, LineStyle::Heading1},   {u"h2"_s, LineStyle::Heading2},
        {u"h3"_s, LineStyle::Heading3}};
    const QString task = m_frontMatter.value(u"task"_s).toString();
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
    } else if (action == u"wikilink") {
        const std::optional<Edit> edit = completeWikilink(working, from, argument);
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
    } else if (action == u"taskNote" && task.isEmpty()) {
        setKey(u"task"_s, u"open"_s);
    } else if (action == u"taskNote") {
        // Only the task key: a due date is the user's own, and comes back with it.
        setKey(u"task"_s, {});
    } else if (action == u"done" && task == u"done") {
        setKey(u"task"_s, u"open"_s);
        setKey(u"done"_s, {});
    } else if (action == u"done") {
        setKey(u"task"_s, u"done"_s);
        setKey(u"done"_s, QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    } else if (action == u"due") {
        if (task.isEmpty())
            setKey(u"task"_s, u"open"_s);
        setKey(u"due"_s, argument.isEmpty() ? QVariant() : QVariant(argument));
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

void MarkdownStyler::setCursorPosition(int position)
{
    if (m_cursorPosition == position)
        return;
    m_cursorPosition = position;
    if (m_highlighter)
        m_highlighter->setCursorPosition(position);
    Q_EMIT cursorPositionChanged();
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
        Q_EMIT imagesChanged();
    });
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
                                      {u"height"_s, image->size.height()}});
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
    m_highlighter->setLook(look);
    scheduleImages();
}

QVariantMap MarkdownStyler::hitAt(int position) const
{
    if (!m_highlighter)
        return {};
    const MarkdownHighlighter::Hit hit = m_highlighter->hitAt(position);
    static const char *kinds[] = {"", "link", "wikilink", "url", "task"};
    return {{u"kind"_s, QLatin1String(kinds[hit.kind])},
            {u"target"_s, hit.target},
            {u"position"_s, hit.position}};
}

} // namespace jot
