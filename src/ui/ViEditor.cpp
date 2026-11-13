#include "ViEditor.h"

#include "KeyRouter.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QHash>
#include <QInputMethod>
#include <QKeyEvent>
#include <QQuickTextDocument>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

const QString kNormal = u"normal"_s;
const QString kInsert = u"insert"_s;
const QString kVisual = u"visual"_s;

/// The TextEdit keys each vi move stands for.
std::optional<std::pair<int, Qt::KeyboardModifiers>> moveKey(const QString &id)
{
    static const QHash<QString, std::pair<int, Qt::KeyboardModifiers>> keys = {
        {u"left"_s, {Qt::Key_Left, Qt::NoModifier}},
        {u"down"_s, {Qt::Key_Down, Qt::NoModifier}},
        {u"up"_s, {Qt::Key_Up, Qt::NoModifier}},
        {u"right"_s, {Qt::Key_Right, Qt::NoModifier}},
        {u"nextWord"_s, {Qt::Key_Right, Qt::ControlModifier}},
        {u"previousWord"_s, {Qt::Key_Left, Qt::ControlModifier}},
        {u"lineStart"_s, {Qt::Key_Home, Qt::NoModifier}},
        {u"lineEnd"_s, {Qt::Key_End, Qt::NoModifier}},
        {u"noteStart"_s, {Qt::Key_Home, Qt::ControlModifier}},
        {u"noteEnd"_s, {Qt::Key_End, Qt::ControlModifier}},
    };
    const auto found = keys.constFind(id);
    if (found == keys.cend())
        return std::nullopt;
    return *found;
}

/// The key as the action table writes it: its text, "Esc", or "Ctrl+" and a letter.
QString keyName(const QKeyEvent *event)
{
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    if (event->key() == Qt::Key_Escape)
        return u"Esc"_s;
    if (modifiers == Qt::ControlModifier && event->key() >= Qt::Key_A && event->key() <= Qt::Key_Z)
        return u"Ctrl+"_s + QChar(event->key());
    if (modifiers & ~Qt::ShiftModifier)
        return {};
    return event->text();
}

} // namespace

ViEditor::ViEditor(QObject *parent) : QObject(parent)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(1000);
    connect(&m_timeout, &QTimer::timeout, this, [this] { setPending({}); });
}

void ViEditor::setTarget(QQuickItem *target)
{
    if (m_target == target)
        return;
    if (m_target) {
        m_target->removeEventFilter(this);
        disconnect(m_target, nullptr, this, nullptr);
    }
    m_target = target;
    if (m_target) {
        m_target->installEventFilter(this);
        for (const char *changed :
             {SIGNAL(cursorPositionChanged()), SIGNAL(selectionStartChanged()),
              SIGNAL(selectionEndChanged()), SIGNAL(textChanged())})
            connect(m_target, changed, this, SIGNAL(placeChanged()));
        connect(m_target, SIGNAL(cursorPositionChanged()), this, SLOT(dropMatches()));
    }
    Q_EMIT targetChanged();
    Q_EMIT placeChanged();
}

void ViEditor::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    setMode(kNormal);
    updateInputMethod();
    Q_EMIT enabledChanged();
}

void ViEditor::setMode(const QString &mode)
{
    if (mode != kNormal && mode != kInsert && mode != kVisual)
        return;
    setPending({});
    if (m_mode == mode)
        return;
    if (m_mode == kVisual && m_target)
        QMetaObject::invokeMethod(m_target, "deselect");
    m_mode = mode;
    endSteps();
    updateInputMethod();
    Q_EMIT modeChanged();
}

bool ViEditor::typing() const
{
    return !m_enabled || m_mode == kInsert;
}

void ViEditor::updateInputMethod()
{
    if (m_target && m_target->hasActiveFocus())
        QGuiApplication::inputMethod()->update(Qt::ImEnabled);
}

void ViEditor::setPending(const QString &pending)
{
    if (pending.isEmpty())
        m_timeout.stop();
    else
        m_timeout.start();
    if (m_pending == pending)
        return;
    m_pending = pending;
    Q_EMIT pendingChanged();
}

bool ViEditor::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_target && event->type() == QEvent::InputMethodQuery && !typing()) {
        // Without text input, the desktop repeats a held key instead of offering accented letters.
        auto *query = static_cast<QInputMethodQueryEvent *>(event);
        for (uint bit = 1; bit != 0 && bit <= query->queries(); bit <<= 1) {
            const auto q = static_cast<Qt::InputMethodQuery>(bit);
            if (query->queries() & q)
                query->setValue(q, q == Qt::ImEnabled ? QVariant(false)
                                                      : m_target->inputMethodQuery(q));
        }
        return true;
    }
    if (!m_enabled || m_sending || watched != m_target || event->type() != QEvent::KeyPress)
        return QObject::eventFilter(watched, event);
    const auto *press = static_cast<QKeyEvent *>(event);
    if (m_mode != kInsert)
        return key(press);
    if (press->key() != Qt::Key_Escape || m_busy ||
        m_target->property("inputMethodComposing").toBool())
        return QObject::eventFilter(watched, event);
    // As in vi, the cursor steps back onto the last character typed.
    QTextCursor at(document());
    at.setPosition(cursor());
    if (!at.atBlockStart())
        setCursor(cursor() - 1);
    setMode(kNormal);
    return true;
}

bool ViEditor::key(const QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Backspace: return move(u"left"_s);
    case Qt::Key_Return:
    case Qt::Key_Enter: return move(u"down"_s);
    case Qt::Key_Delete: return m_mode == kVisual ? run(u"cutSelection"_s) : run(u"deleteChar"_s);
    case Qt::Key_Tab:
    case Qt::Key_Backtab: return true;
    default: break;
    }
    const QString name = keyName(event);
    // Other keys with Ctrl or Alt, such as formatting, and keys with no text, such as the
    // arrows, work as they always do.
    if (name.isEmpty())
        return false;

    // The command line opens on `:`, or on Space when `:` leads.
    const bool colonLeads = m_router && m_router->leaderKey() == u":";
    if (m_mode != kVisual && m_pending.isEmpty() &&
        (!m_router || m_router->pending() != u"leader") && name == (colonLeads ? u" "_s : u":"_s)) {
        Q_EMIT commandLineRequested();
        return true;
    }
    if (m_router && m_router->viMode() && m_pending.isEmpty() &&
        (m_router->pending() == u"leader" || name == m_router->leaderKey())) {
        m_router->press(name);
        return true;
    }
    // The window's own, such as Ctrl+O to go back, work from normal and visual mode too.
    if (m_router && m_router->viMode() && m_pending.isEmpty()) {
        const QString id = KeyRouter::viControlAction(event);
        if (!id.isEmpty()) {
            Q_EMIT m_router->triggered(id);
            return true;
        }
    }
    if (name == u"Esc" && !m_pending.isEmpty()) {
        setPending({});
        return true;
    }

    const QString keys = m_pending + name;
    static const QString kFinds = u"fFtT"_s;
    static const QHash<QChar, QChar> kOther = {
        {u'f', u'F'}, {u'F', u'f'}, {u't', u'T'}, {u'T', u't'}};
    const bool visual = m_mode == kVisual;
    // While a find's matches show, as in flash.nvim, its own key steps on to the next and the
    // other way's key back, as ; and , do.
    if (m_stepping && m_pending.isEmpty() && name.size() == 1 && kFinds.contains(name.front())) {
        const QChar pressed = name.front();
        const bool till = pressed == u't' || pressed == u'T';
        const bool findTill = m_findKind == u't' || m_findKind == u'T';
        if (till == findTill) {
            const bool forward = pressed == u'f' || pressed == u't';
            const bool findForward = m_findKind == u'f' || m_findKind == u't';
            return find({}, forward == findForward ? m_findKind : kOther.value(m_findKind),
                        m_findTarget, true);
        }
    }
    endSteps();
    // After f, F, t or T, any key is the character to go to.
    if (!m_pending.isEmpty() && kFinds.contains(m_pending.back())) {
        const QChar op = m_pending.size() > 1 ? m_pending.front() : QChar();
        const QChar kind = m_pending.back();
        setPending({});
        if (name.size() != 1)
            return true;
        m_findKind = kind;
        m_findTarget = name.front();
        return find(op, kind, m_findTarget, false);
    }
    const bool operated =
        !visual && keys.size() == 2 && (keys.front() == u'd' || keys.front() == u'c');
    const QChar last = keys.back();
    if ((keys.size() == 1 || operated) && kFinds.contains(last)) {
        setPending(keys);
        // Unlike other keys begun, the character to find is waited for.
        m_timeout.stop();
        return true;
    }
    if ((keys.size() == 1 || operated) && (last == u';' || last == u',')) {
        setPending({});
        if (m_findKind.isNull())
            return true;
        const QChar kind = last == u';' ? m_findKind : kOther.value(m_findKind);
        return find(operated ? keys.front() : QChar(), kind, m_findTarget, true);
    }
    if (m_mode == kVisual) {
        const QString id = KeyRouter::viAction(kVisual, keys);
        if (!id.isEmpty()) {
            setPending({});
            return run(id);
        }
    }
    bool partial = false;
    const QString id = KeyRouter::viAction(u"note"_s, keys, &partial);
    if (id.isEmpty() && partial) {
        setPending(keys);
        return true;
    }
    const bool wasPending = !m_pending.isEmpty();
    setPending({});
    if (id.isEmpty()) {
        // A start that leads nowhere lets the next key act on its own.
        if (wasPending)
            return key(event);
        // Ctrl with a letter vi does not use, such as Ctrl+B for bold, works as ever;
        // other letters are not typed.
        return !name.startsWith(u"Ctrl+");
    }
    // Visual mode moves, or ends; the rest of normal mode waits until it does.
    if (m_mode == kVisual && !moveKey(id) && id != u"visual")
        return true;
    run(id);
    return true;
}

bool ViEditor::move(const QString &id)
{
    const auto key = moveKey(id);
    if (!key)
        return false;
    const Qt::KeyboardModifiers modifiers =
        key->second | (m_mode == kVisual ? Qt::ShiftModifier : Qt::NoModifier);
    sendKey(key->first, modifiers);
    return true;
}

bool ViEditor::find(QChar op, QChar kind, QChar target, bool repeat)
{
    QTextDocument *doc = document();
    if (!doc)
        return true;
    const QString text = doc->toPlainText();
    const int from = std::min(cursor(), int(text.size()));
    const bool forward = kind == u'f' || kind == u't';
    const bool till = kind == u't' || kind == u'T';
    const int skip = repeat && till ? 2 : 1;
    const qsizetype found = forward           ? text.indexOf(target, from + skip)
                            : from - skip < 0 ? -1
                                              : text.lastIndexOf(target, from - skip);
    if (found < 0)
        return true;
    const int to = int(found) + (till ? (forward ? -1 : 1) : 0);
    if (op.isNull()) {
        m_finding = true;
        if (m_mode == kVisual)
            QMetaObject::invokeMethod(m_target, "moveCursorSelection", Q_ARG(int, to));
        else
            setCursor(to);
        m_finding = false;
        m_stepping = true;
        setMatches(kind, target);
        return true;
    }
    if (!writable())
        return true;
    // Forward, the character gone to goes too, as in vi.
    const int start = forward ? from : to;
    const int end = forward ? to + 1 : from;
    QTextCursor edit(doc);
    edit.setPosition(start);
    edit.setPosition(end, QTextCursor::KeepAnchor);
    keep(edit.selectedText(), false);
    edit.removeSelectedText();
    setCursor(start);
    if (op == u'c')
        setMode(kInsert);
    return true;
}

void ViEditor::setMatches(QChar kind, QChar target)
{
    QVariantList found;
    if (QTextDocument *doc = document(); doc && !target.isNull()) {
        const QString text = doc->toPlainText();
        const int from = std::min(cursor(), int(text.size()));
        const bool forward = kind == u'f' || kind == u't';
        const bool till = kind == u't' || kind == u'T';
        // Past the character a till stops beside, which it would step over.
        const int start = forward ? from + (till ? 2 : 1) : from - (till ? 2 : 1);
        // Enough to fill any screen; a note full of the character need not light every one.
        constexpr int kMost = 500;
        if (forward) {
            for (qsizetype at = text.indexOf(target, start); at >= 0 && found.size() < kMost;
                 at = text.indexOf(target, at + 1))
                found.append(int(at));
        } else if (start >= 0) {
            for (qsizetype at = text.lastIndexOf(target, start); at >= 0 && found.size() < kMost;
                 at = at > 0 ? text.lastIndexOf(target, at - 1) : -1)
                found.append(int(at));
        }
    }
    if (found == m_matches)
        return;
    m_matches = found;
    Q_EMIT matchesChanged();
}

void ViEditor::endSteps()
{
    m_stepping = false;
    setMatches({}, {});
}

void ViEditor::dropMatches()
{
    if (!m_finding)
        endSteps();
}

void ViEditor::sendKey(int key, Qt::KeyboardModifiers modifiers)
{
    m_sending = true;
    QKeyEvent press(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(m_target, &press);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers);
    QCoreApplication::sendEvent(m_target, &release);
    m_sending = false;
}

bool ViEditor::run(const QString &id)
{
    if (move(id))
        return true;
    QTextDocument *doc = document();
    if (!doc)
        return false;
    QTextCursor edit(doc);
    edit.setPosition(std::min(cursor(), doc->characterCount() - 1));
    const QTextBlock block = edit.block();
    const int blockEnd = block.position() + block.length() - 1;

    if (id == u"insert") {
        setMode(kInsert);
    } else if (id == u"append") {
        if (!edit.atBlockEnd())
            setCursor(edit.position() + 1);
        setMode(kInsert);
    } else if (id == u"insertAtStart" || id == u"appendAtEnd") {
        sendKey(id == u"insertAtStart" ? Qt::Key_Home : Qt::Key_End, Qt::NoModifier);
        setMode(kInsert);
    } else if (id == u"openBelow" || id == u"openAbove") {
        if (!writable())
            return true;
        const bool below = id == u"openBelow";
        edit.setPosition(below ? blockEnd : block.position());
        edit.insertText(u"\n"_s);
        setCursor(below ? edit.position() : block.position());
        setMode(kInsert);
    } else if (id == u"deleteChar") {
        if (!writable() || edit.atBlockEnd())
            return true;
        edit.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        edit.removeSelectedText();
        setCursor(edit.position());
    } else if (id == u"deleteLine") {
        if (!writable())
            return true;
        keep(block.text() + u'\n', true);
        // The line goes with one of the line breaks around it, so no empty line is left.
        if (block.next().isValid()) {
            edit.setPosition(block.position());
            edit.setPosition(block.next().position(), QTextCursor::KeepAnchor);
        } else {
            edit.setPosition(block.previous().isValid() ? block.position() - 1 : block.position());
            edit.setPosition(blockEnd, QTextCursor::KeepAnchor);
        }
        edit.removeSelectedText();
        edit.movePosition(QTextCursor::StartOfBlock);
        setCursor(edit.position());
    } else if (id == u"deleteToEnd") {
        if (!writable())
            return true;
        edit.setPosition(blockEnd, QTextCursor::KeepAnchor);
        keep(edit.selectedText(), false);
        edit.removeSelectedText();
        setCursor(edit.position());
    } else if (id == u"copyLine") {
        keep(block.text() + u'\n', true);
        Q_EMIT yanked(block.position(), blockEnd);
    } else if (id == u"pasteAfter" || id == u"pasteBefore") {
        if (writable())
            paste(id == u"pasteAfter");
    } else if (id == u"undo" || id == u"redo") {
        QMetaObject::invokeMethod(m_target, id == u"undo" ? "undo" : "redo");
    } else if (id == u"visual") {
        setMode(m_mode == kVisual ? kNormal : kVisual);
    } else if (id == u"leave") {
        Q_EMIT leaveRequested();
    } else if (id == u"copySelection" || id == u"cutSelection") {
        const bool cut = id == u"cutSelection" && writable();
        const int start = m_target->property("selectionStart").toInt();
        const int end = m_target->property("selectionEnd").toInt();
        QMetaObject::invokeMethod(m_target, cut ? "cut" : "copy");
        if (!cut)
            Q_EMIT yanked(start, end);
        m_lines.clear();
        setMode(kNormal);
        setCursor(start);
    } else if (id == u"endVisual") {
        setMode(kNormal);
    } else {
        return false;
    }
    return true;
}

void ViEditor::keep(const QString &text, bool lines)
{
    QGuiApplication::clipboard()->setText(text);
    m_lines = lines ? text : QString();
}

void ViEditor::paste(bool after)
{
    const QString text = QGuiApplication::clipboard()->text();
    if (text.isEmpty())
        return;
    QTextCursor edit(document());
    edit.setPosition(cursor());
    if (!m_lines.isEmpty() && text == m_lines) {
        // Whole lines go below or above the cursor's line, and the cursor to their start.
        const QTextBlock block = edit.block();
        if (after) {
            edit.setPosition(block.position() + block.length() - 1);
            edit.insertText(u'\n' + text.chopped(1));
            setCursor(block.position() + block.length());
        } else {
            edit.setPosition(block.position());
            edit.insertText(text);
            setCursor(block.position());
        }
        return;
    }
    if (after && !edit.atBlockEnd())
        edit.movePosition(QTextCursor::NextCharacter);
    edit.insertText(text);
    setCursor(edit.position());
}

QTextDocument *ViEditor::document() const
{
    const auto *text =
        m_target ? m_target->property("textDocument").value<QQuickTextDocument *>() : nullptr;
    return text ? text->textDocument() : nullptr;
}

int ViEditor::line() const
{
    const QTextDocument *text = document();
    return text ? text->findBlock(cursor()).blockNumber() + 1 : 1;
}

int ViEditor::column() const
{
    const QTextDocument *text = document();
    return text ? cursor() - text->findBlock(cursor()).position() + 1 : 1;
}

int ViEditor::percent() const
{
    const QTextDocument *text = document();
    const int last = text ? text->blockCount() - 1 : 0;
    return last > 0 ? qRound(100.0 * (line() - 1) / last) : 0;
}

int ViEditor::pickedLines() const
{
    const QTextDocument *text = document();
    const int start = m_target ? m_target->property("selectionStart").toInt() : 0;
    const int end = m_target ? m_target->property("selectionEnd").toInt() : 0;
    if (!text || end <= start)
        return 0;
    return text->findBlock(end).blockNumber() - text->findBlock(start).blockNumber() + 1;
}

int ViEditor::cursor() const
{
    return m_target ? m_target->property("cursorPosition").toInt() : 0;
}

void ViEditor::setCursor(int position)
{
    if (m_target)
        m_target->setProperty("cursorPosition", position);
}

bool ViEditor::writable() const
{
    return m_target && !m_target->property("readOnly").toBool();
}

QString ViEditor::runCommand(const QString &command)
{
    const QString typed = command.trimmed();
    QTextDocument *text = document();
    if (typed.isEmpty() || !text)
        return {};
    const auto toLine = [this, text](int number) {
        const QTextBlock block =
            text->findBlockByNumber(std::clamp(number, 1, text->blockCount()) - 1);
        setCursor(block.position());
        return QString();
    };
    bool number = false;
    const int line = typed.toInt(&number);
    if (number)
        return toLine(line);
    if (typed == u"$")
        return toLine(text->blockCount());
    const auto trigger = [this](const QString &id) {
        if (m_router)
            Q_EMIT m_router->triggered(id);
    };
    if (typed == u"w") {
        trigger(u"save"_s);
        return {};
    }
    if (typed == u"q") {
        trigger(u"closeTab"_s);
        return {};
    }
    if (typed == u"wq" || typed == u"x") {
        trigger(u"save"_s);
        trigger(u"closeTab"_s);
        return {};
    }
    if (typed == u"noh" || typed == u"nohlsearch") {
        endSteps();
        Q_EMIT highlightsCleared();
        return {};
    }
    const bool everywhere = typed.startsWith(u"%s");
    if (!everywhere && !typed.startsWith(u's'))
        return tr("Not a command: %1").arg(typed);
    // s/old/new/flags, with any character in place of the slash, and a backslash before one
    // meant as is.
    const QString rest = typed.mid(everywhere ? 2 : 1);
    if (rest.isEmpty() || rest.front().isLetterOrNumber() || rest.front().isSpace())
        return tr("Not a command: %1").arg(typed);
    const QChar slash = rest.front();
    QStringList parts{QString()};
    for (qsizetype i = 1; i < rest.size(); ++i) {
        if (rest.at(i) == u'\\' && i + 1 < rest.size() && rest.at(i + 1) == slash) {
            parts.last() += slash;
            ++i;
        } else if (rest.at(i) == slash && parts.size() < 3) {
            parts.append(QString());
        } else {
            parts.last() += rest.at(i);
        }
    }
    if (parts.size() < 2 || parts.first().isEmpty())
        return tr("Not a command: %1").arg(typed);
    const QString flags = parts.size() > 2 ? parts.at(2) : QString();
    QRegularExpression pattern(parts.at(0), flags.contains(u'i')
                                                ? QRegularExpression::CaseInsensitiveOption
                                                : QRegularExpression::NoPatternOption);
    if (!pattern.isValid())
        return tr("Not a pattern: %1").arg(pattern.errorString());
    if (!writable())
        return tr("This note cannot be changed");
    const bool all = flags.contains(u'g');
    const QString after = parts.at(1);
    const QTextBlock first = everywhere ? text->begin() : text->findBlock(cursor());
    const QTextBlock last = everywhere ? text->lastBlock() : first;
    QTextCursor edit(text);
    edit.beginEditBlock();
    int changed = 0;
    for (QTextBlock block = first; block.isValid(); block = block.next()) {
        const QString was = block.text();
        QString now = was;
        if (all) {
            now.replace(pattern, after);
        } else if (const QRegularExpressionMatch m = pattern.match(was); m.hasMatch()) {
            QString one = was.mid(m.capturedStart(), m.capturedLength());
            one.replace(pattern, after);
            now = was.left(m.capturedStart()) + one + was.mid(m.capturedEnd());
        }
        if (now != was) {
            edit.setPosition(block.position());
            edit.setPosition(block.position() + int(was.size()), QTextCursor::KeepAnchor);
            edit.insertText(now);
            ++changed;
        }
        if (block == last)
            break;
    }
    edit.endEditBlock();
    if (changed == 0)
        return tr("Not found: %1").arg(parts.at(0));
    return everywhere ? tr("Replaced on %n line(s)", "", changed) : QString();
}

} // namespace jot
