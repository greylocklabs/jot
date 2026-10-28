pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// The note shown, to read and to write. Its title renames the file; its body is the
/// Markdown itself, styled as it is typed, with a row of formatting over it. Ctrl+click
/// follows a link, and a click on a task's box ticks it.
Rectangle {
    id: root

    /// Where the leader key goes from the note in vi mode.
    property KeyRouter router: null

    /// Escape in vi's normal mode: the keyboard is done with the note.
    signal leftNote

    /// Puts the cursor in the title, all selected, as for a note just made.
    function editTitle() {
        title.forceActiveFocus()
        title.selectAll()
    }

    /// Puts the cursor at the end of the body.
    function editBody() {
        Settings.readMode = false
        body.forceActiveFocus()
        body.cursorPosition = body.length
        vi.mode = "insert"
    }

    /// Puts the cursor back in the body where it was, to type, out of reading.
    function write() {
        Settings.readMode = false
        body.forceActiveFocus()
        vi.mode = "insert"
    }

    /// Formats the selection, or the word or line at the cursor, and keeps the cursor in
    /// the note. Says whether anything was done.
    function format(action, argument) {
        if (action === "source") {
            Settings.readMode = false
            Settings.sourceMode = !Settings.sourceMode
            return true
        }
        if (body.readOnly)
            return false
        const from = body.selectionStart
        const to = body.selectionEnd
        const after = styler.format(action, from, to, argument)
        body.forceActiveFocus()
        if (after.start === undefined)
            return false
        body.select(after.start, after.end)
        return true
    }

    color: Theme.bg

    /// The font notes are written in, as Settings and the theme choose it.
    readonly property string noteFamily: {
        // Read again when the theme changes.
        Theme.name
        return Theme.noteFontFamily(Settings.noteFont)
    }

    // The editor holds the text; the vault hears of each change and saves it shortly after.
    // A note loaded from disk replaces the text without counting as an edit.
    property bool loading: false
    /// Another note is on its way in, rather than this one changed on disk or renamed.
    property bool fresh: true

    /// Opens what a link in the reading view leads to: a note, a tag's notes, or anything
    /// else with the desktop.
    function openLink(link) {
        if (link.startsWith("jot-tag:")) {
            Vault.showingTasks = false
            Vault.notes.tag = decodeURIComponent(link.slice("jot-tag:".length))
        } else if (link.startsWith("jot-note:")) {
            Vault.follow(decodeURIComponent(link.slice("jot-note:".length)), true)
        } else {
            Vault.follow(link, false)
        }
    }

    function load() {
        loading = true
        // Another note starts at its top; the same one, reloaded, stays where it was.
        const another = fresh
        fresh = false
        const cursor = another ? 0 : Vault.movedCursor(body.text, body.cursorPosition)

        // Swapping a whole note in under a live highlighter leaves the TextEdit blank, so
        // the styling comes off for the swap and goes back on after.
        styler.document = null
        body.text = Vault.currentText
        styler.document = body.textDocument
        body.cursorPosition = Math.min(cursor, body.length)
        title.text = Vault.currentTitle
        if (another) {
            flick.contentY = 0
            vi.mode = "normal"
        }
        loading = false
    }

    Connections {
        target: Vault
        function onOpening() {
            root.fresh = true
        }
        function onCurrentChanged() {
            if (root.fresh || Vault.currentText !== body.text || Vault.currentTitle !== title.text)
                root.load()
        }
        function onRevealPosition(position) {
            body.forceActiveFocus()
            body.cursorPosition = Math.min(position, body.length)
        }
    }
    Component.onCompleted: load()

    /// What Return, Tab or Shift+Tab with no other keys would do in a list, or "".
    function listKey(event) {
        const others = event.modifiers & ~(Qt.ShiftModifier | Qt.KeypadModifier)
        if (others !== 0)
            return ""
        const shift = (event.modifiers & Qt.ShiftModifier) !== 0
        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && !shift)
            return "newline"
        if (event.key === Qt.Key_Tab && !shift)
            return "indent"
        if (event.key === Qt.Key_Backtab)
            return "outdent"
        return ""
    }

    /// The formatting a key press asks for, or "". Letters and digits match by key, so they
    /// are the same on every layout; the slash matches by the character typed, with Shift
    /// or without, so layouts that need Shift for it (Shift+7 in German) work too.
    function formatKey(event) {
        const ctrl = (event.modifiers & Qt.ControlModifier) !== 0
        const shift = (event.modifiers & Qt.ShiftModifier) !== 0
        if (!ctrl || (event.modifiers & Qt.AltModifier) !== 0)
            return ""
        if (event.key === Qt.Key_Slash)
            return "source"
        if (shift)
            return event.key === Qt.Key_X ? "strike" : ""
        switch (event.key) {
        case Qt.Key_B:
            return "bold"
        case Qt.Key_I:
            return "italic"
        case Qt.Key_E:
            return "code"
        case Qt.Key_K:
            return "link"
        case Qt.Key_1:
            return "h1"
        case Qt.Key_2:
            return "h2"
        case Qt.Key_3:
            return "h3"
        case Qt.Key_Return:
        case Qt.Key_Enter:
            return "task"
        }
        return ""
    }

    Text {
        anchors.centerIn: parent
        visible: Vault.current === ""
        horizontalAlignment: Text.AlignHCenter
        text: Vault.noteCount === 0 ? qsTr(
                                          "This vault has no notes yet.\nPress Ctrl+N to make one.") :
                                      qsTr("Choose a note, or press Ctrl+P to find one.")
        color: Theme.textFaint
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textBase
    }

    FormatBar {
        id: formatBar

        objectName: "formatBar"
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }
        visible: Vault.current !== "" && Vault.writable
        frontMatter: styler.frontMatter
        onFormatRequested: action => root.format(action, "")
    }

    Flickable {
        id: flick

        objectName: "scroll"

        anchors {
            left: parent.left
            right: parent.right
            top: formatBar.visible ? formatBar.bottom : parent.top
            bottom: parent.bottom
        }
        visible: Vault.current !== ""
        contentWidth: width
        contentHeight: page.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        /// Scrolls so the cursor stays in view while typing. Not before the editor has a
        /// height, when every line would count as out of view.
        function follow(rect) {
            if (height <= 0 || !body.activeFocus)
                return
            const top = page.y + body.y + rect.y
            const bottom = top + rect.height
            if (top < contentY)
                contentY = Math.max(0, top - Theme.space6)
            else if (bottom > contentY + height)
                contentY = bottom - height + Theme.space6
        }

        Column {
            id: page

            width: Math.min(flick.width, Theme.editorWidth + 2 * Theme.space7)
            x: (flick.width - width) / 2
            padding: Theme.space7
            spacing: Theme.space5

            Row {
                width: parent.width - 2 * parent.padding
                spacing: Theme.space3

                TextInput {
                    id: title

                    width: parent.width - trash.width - parent.spacing
                    readOnly: !Vault.writable
                    wrapMode: TextInput.Wrap
                    color: Theme.text
                    selectionColor: Theme.tint(Theme.accent, 0.4)
                    selectedTextColor: Theme.text
                    font.family: Theme.displayFontFamily
                    font.pixelSize: Theme.text2xl
                    font.weight: Theme.weightBold
                    selectByMouse: true
                    Accessible.name: qsTr("Title")
                    onEditingFinished: {
                        if (text.trim() === "")
                            text = Vault.currentTitle
                        else if (text !== Vault.currentTitle)
                            Vault.renameCurrent(text)
                    }
                    Keys.onReturnPressed: root.editBody()
                    Keys.onEscapePressed: {
                        text = Vault.currentTitle
                        root.editBody()
                    }
                }
                DangerButton {
                    id: trash

                    anchors.verticalCenter: title.verticalCenter
                    visible: Vault.writable
                    label: qsTr("Delete")
                    armedLabel: qsTr("Move to trash?")
                    onConfirmed: Vault.trashCurrent()
                }
            }

            TaskDetails {
                visible: Vault.writable && styler.frontMatter.task !== undefined
                frontMatter: styler.frontMatter
                onFormatRequested: (action, argument) => root.format(action, argument)
            }

            Text {
                width: parent.width - 2 * parent.padding
                visible: Vault.problem !== ""
                text: Vault.problem
                wrapMode: Text.WordWrap
                color: Theme.danger
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textMd
                font.weight: Theme.weightStrong
            }

            // The note drawn to read, in place of the text to write in.
            TextEdit {
                id: reader

                objectName: "reader"
                visible: Settings.readMode
                width: parent.width - 2 * parent.padding
                readOnly: true
                // NoteReader draws the document itself; the TextEdit only shows it.
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                color: Theme.text
                selectionColor: Theme.tint(Theme.accent, 0.4)
                selectedTextColor: Theme.text
                font.family: root.noteFamily
                font.pixelSize: Theme.textLg
                selectByMouse: true
                Accessible.name: qsTr("Note, to read")
                onLinkActivated: link => root.openLink(link)

                NoteReader {
                    id: reading

                    document: reader.textDocument
                    text: Settings.readMode ? Vault.currentText : ""
                    fontFamily: root.noteFamily
                    pixelSize: reader.font.pixelSize
                    width: reader.width
                }
                HoverHandler {
                    cursorShape: reader.hoveredLink !== "" ? Qt.PointingHandCursor : Qt.IBeamCursor
                }
                // A click on a checkbox, left of its text, ticks it in the note.
                TapHandler {
                    enabled: Vault.writable
                    onTapped: point => {
                        const position = reader.positionAt(point.position.x, point.position.y)
                        const task = reading.taskAt(position)
                        if (task.line === undefined || point.position.x
                                >= reader.positionToRectangle(position).x)
                            return
                        Vault.setTaskDone(Vault.current, task.line, !task.done, task.text)
                    }
                }
            }

            TextEdit {
                id: body

                objectName: "body"
                visible: !Settings.readMode

                width: parent.width - 2 * parent.padding
                readOnly: !Vault.writable
                wrapMode: TextEdit.Wrap
                textFormat: TextEdit.PlainText
                color: Theme.text
                selectionColor: Theme.tint(Theme.accent, 0.4)
                selectedTextColor: Theme.text
                font.family: root.noteFamily
                font.pixelSize: Theme.textLg
                selectByMouse: true
                persistentSelection: true
                tabStopDistance: 4 * Theme.space5
                Accessible.name: qsTr("Note")
                onTextChanged: {
                    if (root.loading)
                        return
                    Vault.edit(text)
                    // Once the cursor has moved too, so it is read where the typing ended.
                    Qt.callLater(findLink)
                }
                onCursorRectangleChanged: flick.follow(cursorRectangle)
                onActiveFocusChanged: {
                    if (!activeFocus && vi.mode === "visual")
                        vi.mode = "normal"
                }
                // Vi's normal and visual modes show a block on the character; insert mode
                // keeps the TextEdit's own line.
                cursorDelegate: Settings.viMode && vi.mode !== "insert" ? blockCursor : null
                onContentHeightChanged: layoutTick++
                onWidthChanged: layoutTick++

                /// Bumped when lines may have moved, so the images find their lines again.
                property int layoutTick: 0

                /// Where the wikilink being typed starts, or -1, and the one Escape put away.
                property int linkStart: -1
                property int dismissedLink: -1

                /// Offers notes for the wikilink being typed at the cursor, if any. Only typing
                /// asks, so moving through an existing link leaves Return and Tab alone.
                function findLink() {
                    const query = activeFocus && !readOnly && selectionStart === selectionEnd
                          ? styler.wikilinkAt(cursorPosition) : ({})
                    linkStart = query.start === undefined ? -1 : query.start
                    if (linkStart < 0 || linkStart === dismissedLink) {
                        if (linkStart < 0)
                            dismissedLink = -1
                        linkPopup.items = []
                        return
                    }
                    Vault.linkSuggestions.query = query.text
                    linkPopup.items = Vault.linkSuggestions.suggestions(8)
                    linkPopup.highlighted = 0
                }

                function pickLink(index) {
                    const item = linkPopup.items[index]
                    if (item)
                        root.format("wikilink", item.target)
                }

                /// Handles a key while notes are offered; says whether it did.
                function linkKey(event) {
                    const count = linkPopup.items.length
                    switch (event.key) {
                    case Qt.Key_Down:
                        linkPopup.highlighted = (linkPopup.highlighted + 1) % count
                        return true
                    case Qt.Key_Up:
                        linkPopup.highlighted = (linkPopup.highlighted + count - 1) % count
                        return true
                    case Qt.Key_Return:
                    case Qt.Key_Enter:
                    case Qt.Key_Tab:
                        if (event.modifiers !== Qt.NoModifier && event.modifiers
                                !== Qt.KeypadModifier)
                            return false
                        pickLink(linkPopup.highlighted)
                        return true
                    case Qt.Key_Escape:
                        dismissedLink = linkStart
                        linkPopup.items = []
                        return true
                    }
                    return false
                }

                // Moving out of the link being typed puts its notes away.
                onCursorPositionChanged: {
                    if (linkPopup.items.length > 0 && styler.wikilinkAt(cursorPosition).start
                            !== linkStart)
                        linkPopup.items = []
                }

                /// Puts `markdown` on lines of its own at `position`.
                function insertBlock(position, markdown) {
                    const place = Vault.placeBlock(text, position, markdown)
                    insert(place.position, place.text)
                    cursorPosition = place.cursor
                }

                Keys.onPressed: event => {
                    // While notes are offered for a wikilink, the arrows choose one and
                    // Return or Tab puts it in.
                    if (linkPopup.visible && body.linkKey(event)) {
                        event.accepted = true
                        return
                    }
                    // Lists carry on and nest; elsewhere these keys do what they always do.
                    const listAction = root.listKey(event)
                    if (listAction !== "" && !body.inputMethodComposing && root.format(listAction,
                                                                                       "")) {
                        event.accepted = true
                        return
                    }
                    const action = root.formatKey(event)
                    if (action !== "") {
                        root.format(action, "")
                        event.accepted = true
                        return
                    }
                    if (!event.matches(StandardKey.Paste))
                        return
                    // An image on the clipboard becomes an attachment; anything else pastes as text.
                    const markdown = Vault.pasteImage()
                    if (markdown === "")
                        return
                    insertBlock(cursorPosition, markdown)
                    event.accepted = true
                }

                ViEditor {
                    id: vi

                    objectName: "vi"
                    target: body
                    router: root.router
                    enabled: Settings.viMode
                    busy: linkPopup.visible
                    onLeaveRequested: root.leftNote()
                }

                Component {
                    id: blockCursor

                    Rectangle {
                        width: Math.max(2, glyphs.averageCharacterWidth)
                        visible: body.activeFocus
                        color: Theme.accent
                        opacity: Theme.blockCursorOpacity

                        FontMetrics {
                            id: glyphs
                            font: body.font
                        }
                    }
                }

                // The card of notes for a wikilink sits under the cursor, kept inside the note.
                Item {
                    x: Math.max(0, Math.min(body.cursorRectangle.x, body.width - width))
                    y: body.cursorRectangle.y
                    width: Math.min(320, body.width)
                    height: body.cursorRectangle.height

                    SuggestionPopup {
                        id: linkPopup
                        wanted: body.activeFocus
                        onPicked: index => body.pickLink(index)
                    }
                }

                MarkdownStyler {
                    id: styler

                    sourceMode: Settings.sourceMode
                    fontFamily: root.noteFamily
                    cursorPosition: body.activeFocus ? body.cursorPosition : -1
                    pixelSize: body.font.pixelSize
                    width: body.width
                }

                Repeater {
                    model: styler.images

                    Image {
                        required property var modelData

                        readonly property rect line: {
                            body.layoutTick
                            // An edit can shorten the note before the list catches up.
                            return body.positionToRectangle(Math.min(modelData.position,
                                                                     body.length))
                        }

                        x: 0
                        y: line.y
                        width: modelData.width
                        height: modelData.height
                        source: modelData.source
                        sourceSize: Qt.size(modelData.width * 2, modelData.height * 2)
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        smooth: true
                        mipmap: true
                    }
                }

                DropArea {
                    anchors.fill: parent
                    enabled: !body.readOnly
                    keys: ["text/uri-list"]
                    onDropped: drop => {
                        const markdown = Vault.attachFiles(drop.urls)
                        if (markdown === "")
                            return
                        body.insertBlock(body.positionAt(drop.x, drop.y), markdown)
                        drop.accept(Qt.CopyAction)
                    }
                }

                TapHandler {
                    acceptedModifiers: Qt.ControlModifier
                    onTapped: point => {
                        const hit = styler.hitAt(body.positionAt(point.position.x,
                                                                 point.position.y))

                        if (hit.kind === "wikilink" || hit.kind === "link" || hit.kind === "url")
                            Vault.follow(hit.target, hit.kind === "wikilink")
                    }
                }
                TapHandler {
                    acceptedModifiers: Qt.NoModifier
                    enabled: !body.readOnly
                    onTapped: point => {
                        const hit = styler.hitAt(body.positionAt(point.position.x,
                                                                 point.position.y))

                        if (hit.kind !== "task")
                            return
                        const done = body.getText(hit.position, hit.position + 1) !== " "
                        body.remove(hit.position, hit.position + 1)
                        body.insert(hit.position, done ? " " : "x")
                    }
                }
                HoverHandler {
                    id: hover

                    cursorShape: Qt.IBeamCursor
                    onPointChanged: previewTimer.restart()
                    onHoveredChanged: {
                        if (!hovered) {
                            previewTimer.stop()
                            preview.close()
                        }
                    }
                }

                // A link the pointer rests on shows the start of its note.
                Timer {
                    id: previewTimer

                    interval: 450
                    onTriggered: {
                        const at = body.positionAt(hover.point.position.x, hover.point.position.y)
                        const hit = styler.hitAt(at)
                        if (hit.kind !== "wikilink" && hit.kind !== "link") {
                            preview.close()
                            return
                        }
                        const note = Vault.preview(hit.target)
                        if (!note.path) {
                            preview.close()
                            return
                        }
                        preview.title = note.title
                        preview.text = note.text
                        preview.x = Math.min(hover.point.position.x, body.width - preview.width)
                        preview.y = body.positionToRectangle(at).y + body.positionToRectangle(
                                    at).height + Theme.space2
                        preview.open()
                    }
                }

                LinkPreview {
                    id: preview
                }
            }

            Backlinks {
                width: parent.width - 2 * parent.padding
            }
        }
    }

    // Which of vi's modes the note is in, while vi mode is on and the note has the keys.
    Text {
        anchors {
            right: parent.right
            bottom: parent.bottom
            margins: Theme.space4
        }
        visible: Settings.viMode && body.activeFocus
        text: vi.mode === "insert" ? qsTr("Insert") : vi.mode === "visual" ? qsTr("Visual") : vi.pending
                                                                             !== "" ? vi.pending :
                                                                                      qsTr("Normal")
        color: Theme.textFaint
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textSm
        font.weight: Theme.weightHeavy
    }
}
