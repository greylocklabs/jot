#include "KeyRouter.h"

#include "SettingsForeign.h"

#include "jot/Fuzzy.h"

#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>

using namespace Qt::StringLiterals;

namespace jot {

namespace {

// A text field takes its own keys, and so does anything open over the window, such as a
// dialog or a menu.
bool keysTaken(const QQuickWindow *window)
{
    const QQuickItem *item = window ? window->activeFocusItem() : nullptr;
    if (item && (item->inherits("QQuickTextInput") || item->inherits("QQuickTextEdit")))
        return true;
    for (; item; item = item->parentItem()) {
        if (item->inherits("QQuickPopupItem"))
            return true;
    }
    return false;
}

KeyAction act(const QString &id, const QString &label, const QString &scope,
              const QVariantList &standard, const QString &vi = {}, const QString &leader = {})
{
    // The window's actions are shortcuts throughout; in the note, only the formatting ones are,
    // the rest being what a key does there by its nature.
    static const QStringList kNoteShortcuts{u"sourceMode"_s, u"bold"_s,    u"italic"_s,
                                            u"strike"_s,     u"code"_s,    u"link"_s,
                                            u"checkbox"_s,   u"schedule"_s};
    const bool editable = scope == u"window" || (scope == u"note" && kNoteShortcuts.contains(id));
    return {id, label, scope, standard, vi, leader, editable};
}

QVariantMap chosenKeys()
{
    return SettingsForeign::s_instance ? SettingsForeign::s_instance->keys() : QVariantMap();
}

/// A standard shortcut as a key sequence: a StandardKey's own, or the text's.
QList<QKeySequence> sequencesOf(const QVariant &key)
{
    if (key.typeId() == QMetaType::Int)
        return QKeySequence::keyBindings(QKeySequence::StandardKey(key.toInt()));
    return {QKeySequence(key.toString())};
}

/// The same shortcut, Enter for Return and Shift aside for punctuation.
bool sameKey(QKeyCombination a, QKeyCombination b)
{
    const auto normal = [](QKeyCombination c) {
        Qt::Key key = c.key() == Qt::Key_Enter ? Qt::Key_Return : c.key();
        Qt::KeyboardModifiers modifiers = c.keyboardModifiers() & ~Qt::KeypadModifier;
        const bool punctuation = key >= Qt::Key_Exclam && key <= Qt::Key_AsciiTilde &&
                                 !(key >= Qt::Key_0 && key <= Qt::Key_9) &&
                                 !(key >= Qt::Key_A && key <= Qt::Key_Z);
        if (punctuation)
            modifiers &= ~Qt::ShiftModifier;
        return QKeyCombination(modifiers, key);
    };
    return normal(a) == normal(b);
}

} // namespace

KeyRouter::KeyRouter(QObject *parent) : QObject(parent)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, &KeyRouter::cancel);
    connect(this, &KeyRouter::settingsChanged, this, [this] {
        if (!m_viMode)
            cancel();
    });
    if (Settings *settings = SettingsForeign::s_instance)
        connect(settings, &Settings::keysChanged, this, &KeyRouter::actionsChanged);
}

QList<KeyAction> KeyRouter::actions()
{
    using K = QKeySequence;
    const QString window = u"window"_s;
    const QString list = u"list"_s;
    const QString note = u"note"_s;
    const QString visual = u"visual"_s;
    return {
        act(u"newNote"_s, tr("New note"), window, {int(K::New)}, {}, u"n"_s),
        act(u"newFromTemplate"_s, tr("New note from a template"), window, {}, {}, u"N"_s),
        act(u"dailyNote"_s, tr("Today's note"), window, {}, {}, u"j"_s),
        act(u"quickAdd"_s, tr("Add a to-do to the inbox"), window, {u"Ctrl+Shift+A"_s}, {}, u"a"_s),
        act(u"goTo"_s, tr("Go to a note"), window, {u"Ctrl+P"_s}, {}, u"p"_s),
        act(u"commands"_s, tr("Commands"), window, {u"Ctrl+Shift+P"_s}, {}, u"c"_s),
        act(u"search"_s, tr("Search notes"), window, {u"Ctrl+Shift+F"_s}, u"/"_s),
        act(u"find"_s, tr("Find in the note"), window, {int(K::Find)}),
        act(u"replace"_s, tr("Find and replace in the note"), window, {int(K::Replace)}),
        act(u"save"_s, tr("Save now"), window, {int(K::Save)}),
        // Text fields keep these keys for their own undo, so these work everywhere else.
        act(u"undoChange"_s, tr("Undo ticking, trashing or restoring"), window, {int(K::Undo)}),
        act(u"redoChange"_s, tr("Redo ticking, trashing or restoring"), window, {int(K::Redo)}),
        act(u"tasks"_s, tr("Tasks, or back to the notes"), window, {}, {}, u"t"_s),
        act(u"settings"_s, tr("Settings"), window, {int(K::Preferences), u"Ctrl+,"_s}, {}, u"s"_s),
        // F5 alone, not the standard refresh keys: in vi mode Ctrl+R is redo.
        act(u"syncNow"_s, tr("Sync now"), window, {u"F5"_s}),
        act(u"gitCommit"_s, tr("Commit to git..."), window, {}),
        act(u"gitPush"_s, tr("Push to git"), window, {}),
        act(u"help"_s, tr("Keyboard shortcuts"), window, {int(K::HelpContents)}, u"?"_s),
        // Written out: not every platform theme gives the standard Quit key one.
        act(u"quit"_s, tr("Quit Jot"), window, {u"Ctrl+Q"_s}),
        act(u"goBack"_s, tr("Back to the note before"), window, {int(K::Back)}, u"Ctrl+O"_s),
        act(u"goForward"_s, tr("Forward to the note after"), window, {int(K::Forward)},
            u"Ctrl+I"_s),
        act(u"nextNote"_s, tr("Next in the list, or the sidebar"), window, {}, u"j"_s),
        act(u"previousNote"_s, tr("Previous in the list, or the sidebar"), window, {}, u"k"_s),
        act(u"firstNote"_s, tr("First in the list, or the sidebar"), window, {}, u"gg"_s),
        act(u"lastNote"_s, tr("Last in the list, or the sidebar"), window, {}, u"G"_s),
        act(u"editNote"_s, tr("Write in the note"), window, {}, u"i"_s),
        act(u"rename"_s, tr("Rename the note"), window, {}, {}, u"r"_s),
        act(u"closeTab"_s, tr("Close the tab"), window, {int(K::Close)}),
        act(u"nextTab"_s, tr("Next tab"), window, {int(K::NextChild)}),
        act(u"previousTab"_s, tr("Previous tab"), window, {int(K::PreviousChild)}),
        act(u"columns"_s, tr("Notebooks and notes, shown or put away"), window, {u"Ctrl+\\"_s}, {},
            u"b"_s),
        act(u"outline"_s, tr("Outline of the note's headings"), window, {u"Ctrl+Shift+O"_s}, {},
            u"o"_s),
        act(u"focusMode"_s, tr("Focus mode: fade all but the paragraph being written"), window, {},
            {}, u"f"_s),
        act(u"typewriter"_s, tr("Typewriter scrolling: keep the line in the middle"), window, {},
            {}, u"w"_s),
        act(u"lineNumbers"_s, tr("Line numbers beside the note"), window, {}),
        act(u"distractionFree"_s, tr("Fullscreen with only the note"), window, {u"F11"_s}),
        act(u"readMode"_s, tr("Read the note, or write in it"), window, {u"Ctrl+Shift+R"_s}, {},
            u"v"_s),
        // Commands with no keys, for the palette.
        act(u"copyLink"_s, tr("Copy a link to the note"), window, {}),
        act(u"showInFiles"_s, tr("Show the note in Files"), window, {}),
        act(u"record"_s, tr("Record from the microphone, or stop"), window, {}, {}, u"R"_s),
        act(u"export"_s, tr("Export the note..."), window, {}, {}, u"e"_s),
        act(u"history"_s, tr("Past versions of the note"), window, {}, {}, u"h"_s),
        act(u"toggleFavorite"_s, tr("Add the note to favorites, or remove it"), window, {}),
        act(u"importJoplin"_s, tr("Import from Joplin..."), window, {}),
        act(u"viMode"_s, tr("Vi mode on or off"), window, {}),
        act(u"editColors"_s, tr("Edit the theme's colors"), window, {}),
        act(u"update"_s, tr("Update Jot..."), window, {}),
        act(u"whatsNew"_s, tr("What's new"), window, {}),
        act(u"copyDebugInfo"_s, tr("Copy debug info"), window, {}),
        act(u"openLogs"_s, tr("Open logs folder"), window, {}),
        act(u"report"_s, tr("Report a problem..."), window, {}),
        act(u"about"_s, tr("About Jot"), window, {}),
        act(u"support"_s, tr("Support Jot on Ko-fi"), window, {}),
        act(u"trash"_s, tr("Move the selected note or notebook to the trash"), list, {u"Delete"_s},
            {}, u"d"_s),
        act(u"menu"_s, tr("The selected note's or notebook's menu"), list,
            {u"Menu"_s, u"Shift+F10"_s}),

        act(u"sourceMode"_s, tr("Markdown as written, or styled"), note, {u"Ctrl+/"_s}, {}, u"m"_s),
        act(u"bold"_s, tr("Bold"), note, {u"Ctrl+B"_s}),
        act(u"italic"_s, tr("Italic"), note, {u"Ctrl+I"_s}),
        act(u"strike"_s, tr("Strikethrough"), note, {u"Ctrl+Shift+X"_s}),
        act(u"code"_s, tr("Code"), note, {u"Ctrl+E"_s}),
        act(u"link"_s, tr("Link"), note, {u"Ctrl+K"_s}),
        act(u"headings"_s, tr("Headings"), note, {u"Ctrl+1"_s, u"Ctrl+2"_s, u"Ctrl+3"_s}),
        act(u"checkbox"_s, tr("Checkbox"), note, {u"Ctrl+Return"_s}),
        act(u"schedule"_s, tr("The to-do's day and repeat"), note, {u"Ctrl+Shift+D"_s}),
        act(u"listItem"_s, tr("Next list item, or end the list"), note, {u"Return"_s}),
        act(u"lineInItem"_s, tr("New line in a list item"), note, {u"Shift+Return"_s}),
        act(u"nest"_s, tr("Nest or unnest list items"), note, {u"Tab"_s, u"Shift+Tab"_s}),
        act(u"cell"_s, tr("Next or previous table cell"), note, {u"Tab"_s, u"Shift+Tab"_s}),
        act(u"tableRow"_s, tr("New table row, or end the table"), note, {u"Return"_s}),
        act(u"linkTarget"_s, tr("Link to a note, chosen as you type"), note, {u"]("_s}),
        act(u"follow"_s, tr("Follow a link"), note, {u"Ctrl+Click"_s}),
        act(u"pasteImage"_s, tr("Paste an image"), note, {int(K::Paste)}),

        act(u"left"_s, tr("Left"), note, {}, u"h"_s),
        act(u"down"_s, tr("Down a line"), note, {}, u"j"_s),
        act(u"up"_s, tr("Up a line"), note, {}, u"k"_s),
        act(u"right"_s, tr("Right"), note, {}, u"l"_s),
        act(u"nextWord"_s, tr("Next word"), note, {}, u"w"_s),
        act(u"previousWord"_s, tr("Previous word"), note, {}, u"b"_s),
        act(u"lineStart"_s, tr("Start of the line"), note, {}, u"0"_s),
        act(u"lineEnd"_s, tr("End of the line"), note, {}, u"$"_s),
        act(u"noteStart"_s, tr("Start of the note"), note, {}, u"gg"_s),
        act(u"noteEnd"_s, tr("End of the note"), note, {}, u"G"_s),
        act(u"findChar"_s, tr("On to the next character typed, marking those further on"), note, {},
            u"f"_s),
        act(u"findCharBack"_s, tr("Back to the character typed, marking those further back"), note,
            {}, u"F"_s),
        act(u"tillChar"_s, tr("On to just before the next character typed"), note, {}, u"t"_s),
        act(u"tillCharBack"_s, tr("Back to just after the character typed"), note, {}, u"T"_s),
        act(u"repeatFind"_s,
            tr("The last f, F, t or T again, as are f, F, t and T just after one; a comma, when "
               "it is not the leader, goes back"),
            note, {}, u";"_s),
        act(u"insert"_s, tr("Insert before the cursor"), note, {}, u"i"_s),
        act(u"append"_s, tr("Insert after the cursor"), note, {}, u"a"_s),
        act(u"insertAtStart"_s, tr("Insert at the start of the line"), note, {}, u"I"_s),
        act(u"appendAtEnd"_s, tr("Insert at the end of the line"), note, {}, u"A"_s),
        act(u"openBelow"_s, tr("New line below"), note, {}, u"o"_s),
        act(u"openAbove"_s, tr("New line above"), note, {}, u"O"_s),
        act(u"deleteChar"_s, tr("Delete the character"), note, {}, u"x"_s),
        act(u"deleteLine"_s, tr("Cut the line"), note, {}, u"dd"_s),
        act(u"deleteToEnd"_s, tr("Cut to the end of the line"), note, {}, u"D"_s),
        act(u"deleteToChar"_s, tr("Cut to a character, with f, F, t or T after d"), note, {},
            u"df"_s),
        act(u"changeToChar"_s, tr("Change to a character, with f, F, t or T after c"), note, {},
            u"cf"_s),
        act(u"copyLine"_s, tr("Copy the line"), note, {}, u"yy"_s),
        act(u"pasteAfter"_s, tr("Paste after the cursor, or below the line"), note, {}, u"p"_s),
        act(u"pasteBefore"_s, tr("Paste before the cursor, or above the line"), note, {}, u"P"_s),
        act(u"undo"_s, tr("Undo"), note, {}, u"u"_s),
        act(u"redo"_s, tr("Redo"), note, {}, u"Ctrl+R"_s),
        act(u"visual"_s, tr("Select, moving as in normal mode"), note, {}, u"v"_s),
        act(u"leave"_s, tr("Normal mode, then out to the note list"), note, {}, u"Esc"_s),

        act(u"copySelection"_s, tr("Copy the selection"), visual, {}, u"y"_s),
        act(u"cutSelection"_s, tr("Cut the selection"), visual, {}, u"d"_s),
        act(u"endVisual"_s, tr("Back to normal mode"), visual, {}, u"Esc"_s),
    };
}

QList<KeyAction> KeyRouter::chosenActions()
{
    QList<KeyAction> all = actions();
    const QVariantMap chosen = chosenKeys();
    if (chosen.isEmpty())
        return all;
    for (KeyAction &action : all) {
        if (!action.editable || !chosen.contains(action.id))
            continue;
        action.standard.clear();
        for (const QKeySequence &sequence :
             QKeySequence::listFromString(chosen.value(action.id).toString()))
            if (!sequence.isEmpty())
                action.standard << sequence.toString(QKeySequence::PortableText);
    }
    return all;
}

QVariantList KeyRouter::actionList()
{
    return toList(chosenActions());
}

QString KeyRouter::noteAction(int key, int modifiers)
{
    const QKeyCombination pressed{Qt::KeyboardModifiers(modifiers), Qt::Key(key)};
    for (const KeyAction &action : chosenActions()) {
        if (action.scope != u"note" || !action.editable)
            continue;
        for (const QVariant &standard : action.standard) {
            for (const QKeySequence &sequence : sequencesOf(standard)) {
                if (sequence.count() == 1 && sameKey(sequence[0], pressed))
                    return action.id;
            }
        }
    }
    return {};
}

QString KeyRouter::conflict(const QString &id, const QString &sequence)
{
    const QKeySequence wanted(sequence, QKeySequence::PortableText);
    if (wanted.isEmpty())
        return {};
    // Every action's keys count, those that cannot change too, such as the headings' or Paste.
    for (const KeyAction &action : chosenActions()) {
        if (action.id == id)
            continue;
        for (const QVariant &standard : action.standard) {
            for (const QKeySequence &each : sequencesOf(standard)) {
                if (each.count() == wanted.count() && each.count() == 1 &&
                    sameKey(each[0], wanted[0]))
                    return action.label;
                if (each == wanted)
                    return action.label;
            }
        }
    }
    return {};
}

QString KeyRouter::sequenceFrom(int key, int modifiers)
{
    switch (key) {
    case Qt::Key_Control:
    case Qt::Key_Shift:
    case Qt::Key_Alt:
    case Qt::Key_Meta:
    case Qt::Key_AltGr:
    case Qt::Key_unknown: return {};
    default: break;
    }
    const QKeyCombination combination(Qt::KeyboardModifiers(modifiers) & ~Qt::KeypadModifier,
                                      Qt::Key(key));
    return QKeySequence(combination).toString(QKeySequence::PortableText);
}

QString KeyRouter::keyText(const QString &id)
{
    for (const QVariant &action : actionList()) {
        const QVariantMap map = action.toMap();
        if (map.value(u"id"_s).toString() == id) {
            const QStringList keys = map.value(u"keys"_s).toStringList();
            return keys.isEmpty() ? QString() : keys.first();
        }
    }
    return {};
}

QVariantList KeyRouter::commands(const QString &query, const QStringList &unavailable)
{
    struct Scored
    {
        int score;
        KeyAction action;
    };
    QList<Scored> scored;
    const QString pattern = query.simplified().remove(u' ');
    for (const KeyAction &action : chosenActions()) {
        if (action.scope != u"window" || action.id == u"commands" ||
            unavailable.contains(action.id))
            continue;
        if (pattern.isEmpty()) {
            scored.append({0, action});
        } else if (const std::optional<int> score = fuzzyScore(pattern, action.label)) {
            scored.append({*score, action});
        }
    }
    std::ranges::stable_sort(scored,
                             [](const Scored &a, const Scored &b) { return a.score > b.score; });
    QList<KeyAction> found;
    for (const Scored &each : std::as_const(scored))
        found << each.action;
    return toList(found);
}

QVariantList KeyRouter::toList(const QList<KeyAction> &actions)
{
    const auto shown = [](const QVariantList &standard) {
        QStringList keys;
        for (const QVariant &key : standard) {
            if (key.typeId() == QMetaType::Int) {
                const QKeySequence sequence(QKeySequence::StandardKey(key.toInt()));
                if (!sequence.isEmpty())
                    keys << sequence.toString(QKeySequence::NativeText);
                continue;
            }
            // Text that is not a shortcut, such as "](" or "Ctrl+Click", is shown as it is.
            const QKeySequence sequence(key.toString());
            const QString native = sequence.toString(QKeySequence::NativeText);
            keys << (sequence.count() == 1 && !native.isEmpty() ? native : key.toString());
        }
        return keys;
    };
    const QVariantMap chosen = chosenKeys();
    QHash<QString, QVariantList> builtIn;
    for (const KeyAction &action : KeyRouter::actions())
        builtIn.insert(action.id, action.standard);
    QVariantList list;
    for (const KeyAction &action : actions) {
        QStringList portable;
        for (const QVariant &key : action.standard) {
            for (const QKeySequence &sequence : sequencesOf(key))
                portable << sequence.toString(QKeySequence::PortableText);
        }
        list << QVariantMap{{u"id"_s, action.id},
                            {u"label"_s, action.label},
                            {u"scope"_s, action.scope},
                            {u"standard"_s, action.standard},
                            {u"keys"_s, shown(action.standard)},
                            {u"defaultKeys"_s, shown(builtIn.value(action.id))},
                            {u"portable"_s, portable.join(u"; "_s)},
                            {u"editable"_s, action.editable},
                            {u"custom"_s, action.editable && chosen.contains(action.id)},
                            {u"vi"_s, action.vi},
                            {u"leader"_s, action.leader}};
    }
    return list;
}

QString KeyRouter::viAction(const QString &scope, const QString &keys, bool *partial)
{
    QString found;
    bool longer = false;
    for (const KeyAction &action : actions()) {
        if (action.scope != scope || action.vi.isEmpty())
            continue;
        if (action.vi == keys)
            found = action.id;
        else if (action.vi.startsWith(keys))
            longer = true;
    }
    if (partial)
        *partial = longer;
    return found;
}

void KeyRouter::setWindow(QQuickWindow *window)
{
    if (m_window == window)
        return;
    if (m_window)
        m_window->removeEventFilter(this);
    m_window = window;
    if (m_window)
        m_window->installEventFilter(this);
    Q_EMIT windowChanged();
}

QString KeyRouter::viControlAction(const QKeyEvent *event)
{
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    if (modifiers != Qt::ControlModifier || event->key() < Qt::Key_A || event->key() > Qt::Key_Z)
        return {};
    return viAction(u"window"_s, u"Ctrl+"_s + QChar(event->key()));
}

bool KeyRouter::press(const QString &text)
{
    if (!m_viMode || text.isEmpty())
        return false;

    if (m_pending == u"leader") {
        cancel();
        for (const KeyAction &action : actions()) {
            if (action.leader == text) {
                Q_EMIT triggered(action.id);
                break;
            }
        }
        // Whatever followed the leader was meant for it.
        return true;
    }
    if (m_pending.isEmpty() && text == m_leaderKey) {
        setPending(u"leader"_s);
        if (m_leaderTimeout > 0)
            m_timeout.start(m_leaderTimeout);
        return true;
    }

    const QString window = u"window"_s;
    const QString keys = m_pending + text;
    bool partial = false;
    const QString id = viAction(window, keys, &partial);
    if (partial) {
        setPending(keys);
        m_timeout.start(std::max(m_leaderTimeout, 1000));
        return true;
    }
    const bool wasPending = !m_pending.isEmpty();
    cancel();
    if (!id.isEmpty()) {
        Q_EMIT triggered(id);
        return true;
    }
    // A start that leads nowhere lets the next key act on its own.
    return wasPending && press(text);
}

void KeyRouter::cancel()
{
    m_timeout.stop();
    setPending({});
}

void KeyRouter::setPending(const QString &pending)
{
    if (m_pending == pending)
        return;
    m_pending = pending;
    Q_EMIT pendingChanged();
}

bool KeyRouter::eventFilter(QObject *watched, QEvent *event)
{
    // A mouse's back and forward buttons, anywhere, as in a browser.
    if (event->type() == QEvent::MouseButtonPress) {
        const auto *press = static_cast<QMouseEvent *>(event);
        if (press->button() != Qt::BackButton && press->button() != Qt::ForwardButton)
            return QObject::eventFilter(watched, event);
        Q_EMIT triggered(press->button() == Qt::BackButton ? u"goBack"_s : u"goForward"_s);
        return true;
    }
    if (event->type() != QEvent::KeyPress || !m_viMode || keysTaken(m_window))
        return QObject::eventFilter(watched, event);
    const auto *key = static_cast<QKeyEvent *>(event);
    if (key->key() == Qt::Key_Escape && !m_pending.isEmpty()) {
        cancel();
        return true;
    }
    // Shortcuts with Ctrl or Alt are QML's, but for vi's own, and Space presses a focused button
    // unless a command is under way.
    if (key->modifiers() & ~(Qt::ShiftModifier | Qt::KeypadModifier)) {
        const QString id = viControlAction(key);
        if (id.isEmpty())
            return QObject::eventFilter(watched, event);
        cancel();
        Q_EMIT triggered(id);
        return true;
    }
    const QQuickItem *focus = m_window ? m_window->activeFocusItem() : nullptr;
    if (key->key() == Qt::Key_Space && m_pending.isEmpty() && focus &&
        focus->inherits("QQuickAbstractButton"))
        return QObject::eventFilter(watched, event);
    return press(key->text()) || QObject::eventFilter(watched, event);
}

} // namespace jot
