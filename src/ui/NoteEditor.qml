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
    /// Only the note shows, without its format row or tabs.
    property bool distractionFree: false

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

    /// Shows the find bar over the note, with the replace row when `replace`. A table is shown as
    /// its text, which is what is searched.
    function find(replace) {
        Settings.readMode = false
        if (root.tableShown)
            root.showText = true
        findBar.show(replace && Vault.writable)
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
        if (action === "schedule")
            return schedule()
        if (action === "record")
            return record()
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

    /// Sets the day and repeat of the to-do at the cursor: from what its words say, such as
    /// "every week", or else from a popup to type or pick them in. A line that is no to-do
    /// becomes one first.
    function schedule() {
        if (body.readOnly)
            return false
        let at = Vault.taskWordsAt(body.text, body.cursorPosition)
        if (at.start === undefined) {
            styler.format("task", body.cursorPosition, body.cursorPosition, "")
            at = Vault.taskWordsAt(body.text, body.cursorPosition)
            if (at.start === undefined)
                return false
        }
        body.forceActiveFocus()
        const said = Vault.scheduledFromWords(at.words)
        if (said !== "") {
            styler.replace(at.start, at.end, said)
            return true
        }
        scheduler.place = at
        scheduler.openFor(at.words)
        return true
    }

    color: Theme.bg

    /// The font notes are written in, as Settings and the theme choose it.
    readonly property string noteFamily: {
        // Read again when the theme changes.
        Theme.name
        return Theme.noteFontFamily(Settings.noteFont)
    }

    readonly property string monoFamily: {
        Theme.name
        return Theme.noteFontFamily("mono")
    }
    /// A list of records opens as a table; this says to show its text instead.
    property bool showText: false
    readonly property bool tableShown: dataFile && !showText && tableModel.table
    /// A data file, such as JSON or CSV, is edited as plain text, without Markdown's styling.
    readonly property bool dataFile: Vault.currentKind !== "markdown"

    // The editor holds the text; the vault hears of each change and saves it shortly after.
    // A note loaded from disk replaces the text without counting as an edit.
    property bool loading: false
    /// Another note is on its way in, rather than this one changed on disk or renamed.
    property bool fresh: true
    /// The file the editor's text came from. Until the next one is loaded, a change the editor
    /// reports, as when its font changes, belongs to no other file.
    property string loaded

    /// Starts recording from the microphone, or stops and puts the recording in at the cursor.
    function record() {
        if (recorder.recording) {
            recorder.stop()
            return true
        }
        if (!recorder.available || body.readOnly)
            return false
        recordNote = Vault.current
        recordLeft = false
        recorder.start(qsTr("Recording %1").arg(new Date().toLocaleString(Qt.locale(),
                                                                          "yyyy-MM-dd HHmmss")))
        return true
    }
    /// Whether there is a microphone to record from, and whether it is recording, for the
    /// command palette to know.
    readonly property bool canRecord: recorder.available
    readonly property bool recording: recorder.recording
    /// The note the recording goes in: the one it began in, under its name now, or none once
    /// the notes folder changed. It goes in at the cursor as it is when the recording ends, as
    /// what was typed meanwhile moves where it began.
    property string recordNote
    /// Another note was opened while recording, which stops it.
    property bool recordLeft: false

    AudioRecorder {
        id: recorder

        // In the note it began in, at the cursor, or at that note's end once another is shown.
        onFinished: url => {
            const note = root.recordNote
            root.recordNote = ""
            root.recordLeft = false
            if (note === "") {
                recordingProblem.text = qsTr(
                            "The recording was not kept, as the notes folder changed.")
                return
            }
            if (Vault.current !== note) {
                if (!Vault.attachToNoteEnd(note, url))
                    recordingProblem.text = qsTr("The recording could not be added to %1.").arg(
                                Vault.titleOf(note))
                return
            }
            const markdown = Vault.attachFiles([url])
            if (markdown !== "")
                body.insertBlock(body.cursorPosition, markdown)
        }
        onFailed: why => recordingProblem.text = qsTr("Could not record: %1").arg(why)
    }
    // Leaving the note stops recording into it.
    Connections {
        target: Vault
        function onOpening() {
            if (!recorder.recording)
                return
            root.recordLeft = true
            recorder.stop()
        }
        // A rename keeps the note, so the recording follows it.
        function onCurrentChanged() {
            if (root.recordNote !== "" && !root.recordLeft && Vault.current !== "")
                root.recordNote = Vault.current
        }
        function onOpenChanged() {
            if (!recorder.recording)
                return
            root.recordNote = ""
            recorder.stop()
        }
    }

    /// Brings the heading numbered `index`, from 0, to the top, with the cursor at its start
    /// when writing.
    function goToHeading(index) {
        if (Settings.readMode) {
            const position = reading.headingPosition(index)
            if (position >= 0)
                scrollTo(reader.y + reader.positionToRectangle(position).y)
            return
        }
        const heading = styler.headings[index]
        if (!heading)
            return
        body.forceActiveFocus()
        body.cursorPosition = heading.position
        scrollTo(body.y + body.positionToRectangle(heading.position).y)
    }

    /// Scrolls so `y`, in the page, is near the top.
    function scrollTo(y) {
        flick.contentY = Math.max(0, Math.min(page.y + y - Theme.space6, flick.contentHeight
                                              - flick.height))
    }

    /// Opens what a link in the reading view leads to: a note, a tag's view, or anything
    /// else with the desktop.
    function openLink(link) {
        if (link.startsWith("jot-tag:")) {
            Vault.shownTag = decodeURIComponent(link.slice("jot-tag:".length))
        } else if (link.startsWith("jot-note:")) {
            Vault.follow(decodeURIComponent(link.slice("jot-note:".length)), true)
        } else if (link.startsWith("jot-wiki-sound:")) {
            Vault.follow(decodeURIComponent(link.slice("jot-wiki-sound:".length)), true)
        } else if (link.startsWith("jot-sound:")) {
            Vault.follow(decodeURIComponent(link.slice("jot-sound:".length)), false)
        } else if (link.startsWith("jot-heading:")) {
            // A table of contents entry scrolls its heading to the top.
            goToHeading(Number(link.slice("jot-heading:".length)))
        } else {
            Vault.follow(link, false)
        }
    }

    function load() {
        loading = true
        // Another note starts at its top, or where it was left when gone back to; the same one,
        // reloaded, stays where it was.
        const another = fresh
        fresh = false
        const cursor = another ? Vault.openingCursor : Vault.movedCursor(body.text,
                                                                         body.cursorPosition)

        // Swapping a whole note in under a live highlighter leaves the TextEdit blank, so
        // the styling comes off for the swap and goes back on after.
        styler.document = null
        dataStyler.document = null
        body.text = Vault.currentText
        if (dataFile)
            dataStyler.document = body.textDocument
        else
            styler.document = body.textDocument
        // The kind first, so the text is read as what it is, not as the last file was.
        tableModel.kind = dataFile ? Vault.currentKind : ""
        tableModel.text = Vault.currentText
        // What opens as text stays text while it is typed into, until a table is asked for.
        if (another)
            showText = !tableModel.table
        loaded = Vault.current
        body.cursorPosition = Math.min(cursor, body.length)
        title.show(Vault.currentTitle)
        if (another) {
            flick.contentY = 0
            vi.mode = "normal"
            if (body.cursorPosition > 0)
                Qt.callLater(() => flick.reveal(body.cursorRectangle))
        }
        Vault.cursorPosition = body.cursorPosition
        loading = false
    }

    // The note's text as the editor holds it, which turns \r\n into plain lines.
    function shown(text) {
        return text.replace(/\r\n/g, "\n")
    }

    Connections {
        target: Vault
        function onOpening() {
            root.fresh = true
        }
        function onCurrentChanged() {
            if (root.fresh || root.shown(Vault.currentText) !== body.text || Vault.currentTitle
                    !== title.text)
                root.load()
            else
                root.loaded = Vault.current
            // moved, with nothing in it changed
        }
        // As when its `title:` key is edited, here or on disk. Left alone only while something
        // else is being typed in it, so a stale title is never there to be saved back.
        function onCurrentTitleChanged() {
            if (!title.activeFocus || title.text === title.shown)
                title.show(Vault.currentTitle)
        }
        // A to-do ticked from the task view, made here as an edit so the editor's undo history
        // keeps it, and what came before, instead of the whole text being swapped.
        function onCurrentPatched(start, length, text) {
            if (root.loading)
                return
            const patched = body.text.slice(0, start) + text + body.text.slice(start + length)
            if (patched !== root.shown(Vault.currentText))
                return
            styler.replace(start, start + length, text)
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
            return event.key === Qt.Key_X ? "strike" : event.key === Qt.Key_D ? "schedule" : ""
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

    NoteTabs {
        id: noteTabs

        objectName: "noteTabs"
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }
        visible: Vault.tabs.length > 1 && !root.distractionFree
        height: visible ? implicitHeight : 0
    }

    FormatBar {
        id: formatBar

        objectName: "formatBar"
        anchors {
            left: parent.left
            right: parent.right
            top: noteTabs.bottom
        }
        visible: Vault.current !== "" && Vault.writable && !root.dataFile && !root.distractionFree
        frontMatter: styler.frontMatter
        canRecord: recorder.available
        recording: recorder.recording
        onFormatRequested: action => root.format(action, "")
    }

    Flickable {
        id: flick

        objectName: "scroll"

        anchors {
            left: parent.left
            right: outline.visible && !outline.floating ? outline.left : parent.right
            top: formatBar.visible ? formatBar.bottom : noteTabs.bottom
            bottom: wordCount.visible ? wordCount.top : statusLine.visible ? statusLine.top :
                                                                             parent.bottom
        }
        visible: Vault.current !== ""
        contentWidth: width
        contentHeight: page.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        // A note that grows under the cursor, as with Return on its last line, is centered
        // again once its new height is known.
        onContentHeightChanged: {
            if (Settings.typewriter)
                follow(body.cursorRectangle)
        }
        /// Scrolls so the cursor stays in view while typing. Not before the editor has a
        /// height, when every line would count as out of view.
        function follow(rect) {
            if (!body.activeFocus)
                return
            // Typewriter scrolling keeps the line being written in the middle.
            if (Settings.typewriter && !root.dataFile && height > 0)
                contentY = Math.max(0, Math.min(page.y + body.y + rect.y + rect.height / 2 - height
                                                / 2, contentHeight - height))
            else
                reveal(rect)
        }
        /// Scrolls so `rect`, in the body, is in view.
        function reveal(rect) {
            if (height <= 0)
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

            /// Room kept on the right for comments in the margin, when the note has some.
            readonly property real marginRoom: !root.dataFile && styler.comments.length > 0
                                               && flick.width >= 640 ? 260 : 0

            width: root.dataFile ? flick.width : Math.min(flick.width - marginRoom,
                                                          Theme.editorWidth + 2 * Theme.space7)
            x: Math.max(0, Math.min((flick.width - width) / 2, flick.width - width - marginRoom))
            padding: Theme.space7
            // Room under the last line for typewriter scrolling to bring it to the middle.
            bottomPadding: Settings.typewriter && !root.dataFile ? Math.max(Theme.space7,
                                                                            flick.height / 2) :
                                                                   Theme.space7
            spacing: Theme.space5

            Row {
                width: parent.width - 2 * parent.padding
                spacing: Theme.space3

                // The buttons beside the title line up with its first line's letters, not the
                // middle of the whole title, which a long one wraps and the font's descent pulls
                // down.
                FontMetrics {
                    id: titleMetrics
                    font: title.font
                }
                // The title at its full size, and how wide its widest unbreakable stretch is in it,
                // for what gives way when the note is narrow.
                readonly property font titleFont: Qt.font({
                                                              "family": Theme.displayFontFamily,
                                                              "pixelSize": Theme.text2xl,
                                                              "weight": Theme.weightBold
                                                          })
                readonly property real widestWord: Theme.widestUnbreakable(title.text, titleFont)
                readonly property real firstLineMiddle: {
                    const letters = titleMetrics.tightBoundingRect("H")
                    return title.y + titleMetrics.ascent + letters.y + letters.height / 2
                }

                TextInput {
                    id: title
                    objectName: "title"

                    /// The title as last shown, to tell one typed in from one only clicked into.
                    property string shown

                    function show(value) {
                        text = value
                        shown = value
                    }

                    width: parent.width - trash.width - parent.spacing - (viewSwitch.visible
                                                                          ? viewSwitch.width
                                                                            + parent.spacing : 0) - (
                               unsavedMark.visible ? unsavedMark.width + parent.spacing : 0)
                    readOnly: !Vault.writable
                    wrapMode: TextInput.Wrap
                    color: Theme.text
                    selectionColor: Theme.tint(Theme.accent, 0.4)
                    selectedTextColor: Theme.text
                    font.family: parent.titleFont.family
                    // Smaller, down to the next size, when a word would not fit whole.
                    font.pixelSize: Theme.fittingPixelSize(text, parent.titleFont, width,
                                                           Theme.textXl)
                    font.weight: parent.titleFont.weight
                    selectByMouse: true
                    Accessible.name: qsTr("Title")
                    onEditingFinished: {
                        if (text.trim() === "" || text === shown)
                            show(Vault.currentTitle)
                        else if (text !== Vault.currentTitle)
                            Vault.renameCurrent(text)
                    }
                    Keys.onReturnPressed: root.editBody()
                    Keys.onEscapePressed: {
                        show(Vault.currentTitle)
                        root.editBody()
                    }
                }
                // With saving manual, typing not yet saved.
                Rectangle {
                    id: unsavedMark

                    objectName: "unsavedMark"
                    y: parent.firstLineMiddle - height / 2
                    width: 10
                    height: 10
                    radius: 5
                    visible: Vault.unsaved && !Settings.autosave
                    color: Theme.accent
                    Accessible.name: qsTr("Not saved")

                    HoverHandler {
                        id: unsavedHover
                    }
                    Tip {
                        visible: unsavedHover.hovered
                        text: qsTr("Not saved yet. Ctrl+S saves.")
                    }
                }
                // A list of records shows as a table or as its text.
                WhyDisabled {
                    y: parent.firstLineMiddle - height / 2
                    visible: root.dataFile
                    target: viewSwitch
                    reason: qsTr("This can't be a table: %1.").arg(tableModel.why)

                    StickerButton {
                        id: viewSwitch

                        objectName: "viewSwitch"
                        // Only text that reads as a list of records can be a table.
                        enabled: root.tableShown || tableModel.table
                        text: root.tableShown ? qsTr("Edit as text") : qsTr("Edit as table")
                        onClicked: {
                            if (root.tableShown) {
                                root.showText = true
                            } else {
                                tableModel.text = body.text
                                root.showText = !tableModel.table
                            }
                        }
                    }
                }
                DangerButton {
                    id: trash

                    /// Too little room beside it for the title's longest word: a trash can
                    /// alone, so the title keeps its words whole.
                    readonly property bool compact: {
                        return parent.width - parent.spacing - wideTrash.width - (
                                    viewSwitch.visible ? viewSwitch.width + parent.spacing : 0) - (
                                    unsavedMark.visible ? unsavedMark.width + parent.spacing : 0)
                                < parent.widestWord
                    }

                    objectName: "trash"
                    y: parent.firstLineMiddle - height / 2
                    visible: Vault.writable
                    label: compact ? "" : qsTr("Delete")
                    armedLabel: compact ? "" : qsTr("Move to trash?")
                    glyph: compact ? "trash" : ""
                    tip: compact ? (armed ? qsTr("Click again to move to trash") : qsTr("Delete")) :
                                   ""
                    Accessible.name: armed ? qsTr("Move to trash?") : qsTr("Delete")

                    // How wide the button is with its words, to tell whether they fit.
                    StickerButton {
                        id: wideTrash

                        visible: false
                        text: qsTr("Move to trash?")
                    }
                    // Other notes' links to its attachments are worth a question first.
                    onConfirmed: {
                        if (Vault.notesUsingAttachments(Vault.current).length > 0)
                            trashConfirm.askForNote(Vault.current)
                        else
                            Vault.trashCurrent()
                    }
                }
            }

            // The note's file in the vault, for debugging, with developer mode on.
            Row {
                objectName: "notePath"
                visible: Settings.developerMode && Vault.current !== ""
                width: parent.width - 2 * parent.padding
                spacing: Theme.space2

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - copyPath.width - parent.spacing
                    text: Vault.current
                    textFormat: Text.PlainText
                    elide: Text.ElideMiddle
                    color: Theme.textMuted
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.textXs
                }
                PillButton {
                    id: copyPath
                    anchors.verticalCenter: parent.verticalCenter
                    label: qsTr("Copy")
                    Accessible.name: qsTr("Copy the path")
                    onClicked: Support.copyText(Vault.current)
                }
            }

            // Another version of this note was kept beside it, and waits to be looked at.
            Rectangle {
                id: conflictBanner

                objectName: "conflictBanner"
                readonly property var copies: Vault.currentConflicts
                readonly property var first: copies.length > 0 ? copies[0] : null

                width: parent.width - 2 * parent.padding
                height: bannerRow.implicitHeight + 2 * Theme.space3
                visible: first !== null
                radius: Theme.radiusMd
                color: Theme.tint(Theme.accent, 0.15)
                border.color: Theme.accent

                Row {
                    id: bannerRow

                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                        margins: Theme.space3
                    }
                    spacing: Theme.space3

                    Text {
                        width: parent.width - compare.width - parent.spacing
                        anchors.verticalCenter: parent.verticalCenter
                        wrapMode: Text.Wrap
                        text: {
                            const banner = conflictBanner
                            if (!banner.first)
                                return ""
                            const where = [banner.first.from !== "" ? qsTr("from %1").arg(
                                                                          banner.first.from) : "",
                                           banner.first.when].filter(part => part !== "").join(", ")
                            const kept = banner.copies.length === 1 ? qsTr(
                                                                          "Another version of this note was kept beside it") :
                                                                      qsTr("%1 other versions of this note were kept beside it").arg(
                                                                          banner.copies.length)
                            return where !== "" ? kept + " (" + where + ")." : kept + "."
                        }
                        // Names in it come from file names, which another computer may have chosen.
                        textFormat: Text.PlainText
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                    StickerButton {
                        id: compare

                        objectName: "compareConflict"
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Compare...")
                        onClicked: resolver.openFor(conflictBanner.first)
                    }
                }
            }

            TaskDetails {
                visible: Vault.writable && !root.dataFile && styler.frontMatter.task !== undefined
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

            // Where a data file stops reading as its kind; a click goes there.
            Text {
                id: dataProblem

                objectName: "dataProblem"
                width: parent.width - 2 * parent.padding
                visible: root.dataFile && dataStyler.problem.line !== undefined
                text: visible ? qsTr("Line %1, column %2: %3").arg(dataStyler.problem.line).arg(
                                    dataStyler.problem.column).arg(dataStyler.problem.message) : ""
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                color: Theme.danger
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSm
                font.weight: Theme.weightStrong

                TapHandler {
                    onTapped: {
                        body.forceActiveFocus()
                        body.cursorPosition = Math.min(dataStyler.problemPosition(), body.length)
                    }
                }
                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }
            }

            DataTableView {
                id: tableView

                objectName: "tableView"
                visible: root.tableShown
                width: parent.width - 2 * parent.padding
                // The rest of the window below the title, scrolled inside the table.
                height: Math.max(240, flick.height - title.height - 2 * page.padding - page.spacing)
                records: tableModel
            }

            // The note drawn to read, in place of the text to write in.
            TextEdit {
                id: reader

                objectName: "reader"
                visible: Settings.readMode && !root.dataFile
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
                // Ctrl+/ goes back to writing, in the mode it switches to, as it does there.
                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Slash && event.modifiers === Qt.ControlModifier) {
                        root.format("source", "")
                        event.accepted = true
                    } else if (event.matches(StandardKey.Copy) && reader.selectedText !== "") {
                        // Without the characters standing in for task boxes and pictures.
                        reading.copy(reader.selectionStart, reader.selectionEnd)
                        event.accepted = true
                    }
                }

                NoteReader {
                    id: reading
                    objectName: "reading"

                    document: reader.textDocument
                    text: Settings.readMode && !root.dataFile ? Vault.currentText : ""
                    fontFamily: root.noteFamily
                    pixelSize: reader.font.pixelSize
                    width: reader.width
                }
                HoverHandler {
                    cursorShape: reader.hoveredLink !== "" ? Qt.PointingHandCursor : Qt.IBeamCursor
                }
                // A sound embedded on a line of its own plays there, as in the editor.
                Repeater {
                    id: readPlayers

                    model: reading.sounds.length

                    AudioPlayer {
                        required property int index
                        readonly property var sound: reading.sounds[index] ?? ({})
                        readonly property rect line: {
                            reader.width
                            reader.contentHeight
                            return reader.positionToRectangle(sound.position ?? 0)
                        }

                        objectName: "readAudioPlayer"
                        x: line.x
                        y: line.y + (line.height - height) / 2
                        width: sound.width ?? 0
                        height: sound.height ?? 0
                        source: sound.source ?? ""
                    }
                }
                // A click on a checkbox, left of its text, ticks it in the note.
                TapHandler {
                    enabled: Vault.writable
                    onTapped: point => {
                        const position = reader.positionAt(point.position.x, point.position.y)
                        const task = reading.taskAt(position)
                        if (task.line === undefined || point.position.x
                                >= reader.positionToRectangle(task.textStart).x)
                            return
                        Vault.setTaskDone(Vault.current, task.line, !task.done, task.text)
                    }
                }
            }

            TextEdit {
                id: body

                objectName: "body"
                visible: (!Settings.readMode || root.dataFile) && !root.tableShown

                width: parent.width - 2 * parent.padding
                readOnly: !Vault.writable
                wrapMode: TextEdit.Wrap
                textFormat: TextEdit.PlainText
                color: Theme.text
                selectionColor: Theme.tint(Theme.accent, 0.4)
                selectedTextColor: Theme.text
                font.family: root.dataFile ? root.monoFamily : root.noteFamily
                font.pixelSize: root.dataFile ? Theme.textBase : Theme.textLg
                // Room for line numbers beside a data file.
                leftPadding: root.dataFile ? gutter.width + Theme.space3 : 0
                selectByMouse: true
                persistentSelection: true
                tabStopDistance: 4 * Theme.space5
                Accessible.name: qsTr("Note")
                onTextChanged: {
                    if (root.loading || root.loaded !== Vault.current)
                        return
                    Vault.edit(text)
                    if (root.dataFile && !root.tableShown)
                        tableCheck.restart()
                    // Once the cursor has moved too, so it is read where the typing ended.
                    Qt.callLater(findLink)
                    Qt.callLater(findEmoji)
                    Qt.callLater(findMarker)
                }
                onCursorRectangleChanged: flick.follow(cursorRectangle)
                onActiveFocusChanged: {
                    if (!activeFocus && vi.mode === "visual")
                        vi.mode = "normal"
                    // Leaving the note tidies the line, as leaving the line does.
                    if (!activeFocus && cursorLine >= 0 && !readOnly && !root.loading)
                        styler.tidyTaskLine(cursorLine)
                }
                // One cursor for every vi mode, never swapped for the built-in one, which can be
                // left drawn where it was.
                cursorDelegate: Settings.viMode ? blockCursor : null
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

                /// Where the emoji name being typed starts, at its colon, or -1, and the one
                /// Escape put away.
                property int emojiStart: -1
                property int dismissedEmoji: -1

                /// Offers emoji for the name being typed after a colon, as for wikilinks.
                function findEmoji() {
                    const query = activeFocus && !readOnly && selectionStart === selectionEnd
                          && linkStart < 0 ? styler.emojiAt(cursorPosition) : ({})
                    emojiStart = query.start === undefined ? -1 : query.start
                    if (emojiStart < 0 || emojiStart === dismissedEmoji) {
                        if (emojiStart < 0)
                            dismissedEmoji = -1
                        emojiPopup.items = []
                        return
                    }
                    emojiPopup.items = Emoji.suggestions(query.text, 8).map(each => ({
                        "text": each.emoji,
                        "detail": each.name
                    }))
                    emojiPopup.highlighted = 0
                }

                /// Where the to-do marker being typed starts, at its @, or -1, and the one Escape
                /// put away.
                property int markerStart: -1
                property int dismissedMarker: -1

                /// Offers to-do markers, @weekly, @due and the rest, after an @ on a to-do's line.
                function findMarker() {
                    const query = activeFocus && !readOnly && selectionStart === selectionEnd
                          && linkStart < 0 && emojiStart < 0 ? styler.markerAt(cursorPosition) : (
                                                                   {})
                    markerStart = query.start === undefined ? -1 : query.start
                    if (markerStart < 0 || markerStart === dismissedMarker) {
                        if (markerStart < 0)
                            dismissedMarker = -1
                        markerPopup.items = []
                        return
                    }
                    markerPopup.items = styler.markerSuggestions(query.text).map(each => ({
                        "text": each.shown,
                        "detail": each.detail,
                        "marker": each.text
                    }))
                    markerPopup.highlighted = 0
                }

                /// Writes the marker chosen, with a space after a whole one.
                function pickMarker(index) {
                    const item = markerPopup.items[index]
                    if (!item)
                        return
                    const start = markerStart
                    const marker = item.marker.endsWith(" ") ? item.marker : item.marker + " "
                    markerPopup.items = []
                    dismissedMarker = start
                    styler.replace(start, cursorPosition, marker)
                    cursorPosition = start + marker.length
                }

                /// Puts the emoji itself in place of the colon and name, not a shortcode.
                function pickEmoji(index) {
                    const item = emojiPopup.items[index]
                    if (!item)
                        return
                    const start = emojiStart
                    emojiPopup.items = []
                    styler.replace(start, cursorPosition, item.text)
                    cursorPosition = start + item.text.length
                }

                /// Handles a key while `popup` offers something; says whether it did.
                function suggestionKey(event, popup, pick, dismiss) {
                    const count = popup.items.length
                    switch (event.key) {
                    case Qt.Key_Down:
                        popup.highlighted = (popup.highlighted + 1) % count
                        return true
                    case Qt.Key_Up:
                        popup.highlighted = (popup.highlighted + count - 1) % count
                        return true
                    case Qt.Key_Return:
                    case Qt.Key_Enter:
                    case Qt.Key_Tab:
                        if (event.modifiers !== Qt.NoModifier && event.modifiers
                                !== Qt.KeypadModifier)
                            return false
                        pick(popup.highlighted)
                        return true
                    case Qt.Key_Escape:
                        dismiss()
                        return true
                    }
                    return false
                }

                // Moving out of the link being typed puts its notes away.
                /// The line the cursor was on, to tidy once it leaves it.
                property int cursorLine: -1

                /// Writes a to-do's days said in words, such as "@done yesterday", as dates on
                /// the line just left, where nothing moves under the cursor.
                function tidyLeftLine() {
                    const line = styler.lineAt(cursorPosition)
                    if (cursorLine >= 0 && line !== cursorLine && !readOnly && !root.loading)
                        styler.tidyTaskLine(cursorLine)
                    cursorLine = line
                }

                onCursorPositionChanged: {
                    tidyLeftLine()
                    if (!root.loading)
                        Vault.cursorPosition = cursorPosition
                    if (linkPopup.items.length > 0 && styler.wikilinkAt(cursorPosition).start
                            !== linkStart)
                        linkPopup.items = []
                    if (emojiPopup.items.length > 0 && styler.emojiAt(cursorPosition).start
                            !== emojiStart)
                        emojiPopup.items = []
                    if (markerPopup.items.length > 0 && styler.markerAt(cursorPosition).start
                            !== markerStart)
                        markerPopup.items = []
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
                    if (linkPopup.visible && body.suggestionKey(event, linkPopup, body.pickLink, ()
                                                                => {
                                                                    body.dismissedLink
                                                                            = body.linkStart
                                                                    linkPopup.items = []
                                                                })) {
                        event.accepted = true
                        return
                    }
                    // And the same for emoji after a colon.
                    if (emojiPopup.visible && body.suggestionKey(event, emojiPopup, body.pickEmoji, (
                                                                     ) => {
                                                                         body.dismissedEmoji
                                                                                 = body.emojiStart
                                                                         emojiPopup.items = []
                                                                     })) {
                        event.accepted = true
                        return
                    }
                    // And for to-do markers after an @.
                    if (markerPopup.visible && body.suggestionKey(event, markerPopup,
                                                                  body.pickMarker, () => {
                                                                      body.dismissedMarker
                                                                              = body.markerStart
                                                                      markerPopup.items = []
                                                                  })) {
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
                    onYanked: (start, end) => styler.flash(start, end)
                    onMatchesChanged: styler.setMatches(matches)
                }

                // The text is read for a table once typing pauses, so Edit as table can say
                // whether it would work.
                Timer {
                    id: tableCheck
                    interval: 300
                    onTriggered: {
                        if (root.dataFile && !root.tableShown)
                            tableModel.text = body.text
                    }
                }

                DataTableModel {
                    id: tableModel

                    path: root.dataFile ? Vault.root + "/" + Vault.current : ""
                    // An edit in the table is the file's new text, which the text view shows too.
                    onEdited: text => {
                        root.loading = true
                        body.text = text
                        root.loading = false
                        Vault.edit(text)
                    }
                }

                DataStyler {
                    id: dataStyler

                    kind: root.dataFile ? Vault.currentKind : ""
                }

                // A data file's line numbers, for the lines in view.
                Item {
                    id: gutter

                    objectName: "gutter"
                    z: -1
                    visible: root.dataFile
                    width: digits.advanceWidth(String(Math.max(body.lineCount, 99)))
                    height: body.height

                    FontMetrics {
                        id: digits
                        font: body.font
                    }
                    Repeater {
                        model: {
                            body.layoutTick
                            if (!root.dataFile || !dataStyler.document)
                                return []
                            const top = flick.contentY - page.y - body.y
                            return dataStyler.lineTops(top, top + flick.height)
                        }

                        Text {
                            required property var modelData

                            width: gutter.width
                            y: body.topPadding + modelData.y
                            horizontalAlignment: Text.AlignRight
                            text: modelData.number
                            color: Theme.textFaint
                            font: body.font
                        }
                    }
                }

                // The bracket by the cursor and the one it pairs with.
                Repeater {
                    model: root.dataFile && body.activeFocus ? dataStyler.brackets(
                                                                   body.cursorPosition) : []

                    Rectangle {
                        required property int modelData
                        readonly property rect at: {
                            body.layoutTick
                            return body.positionToRectangle(modelData)
                        }

                        z: -1
                        x: at.x
                        y: at.y
                        width: digits.averageCharacterWidth
                        height: at.height
                        radius: Theme.radiusSm
                        color: Theme.tint(Theme.accent, 0.35)
                    }
                }

                Component {
                    id: blockCursor

                    // A block over the character, or a bar before it while inserting.
                    Rectangle {
                        objectName: "blockCursor"
                        width: vi.mode === "insert" ? 2 : Math.max(2, glyphs.averageCharacterWidth)
                        visible: body.activeFocus
                        color: Theme.accent
                        opacity: vi.mode === "insert" ? 1 : Theme.blockCursorOpacity

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
                    SuggestionPopup {
                        id: emojiPopup
                        objectName: "emojiPopup"
                        wanted: body.activeFocus
                        onPicked: index => body.pickEmoji(index)
                    }
                    SuggestionPopup {
                        id: markerPopup
                        objectName: "markerPopup"
                        wanted: body.activeFocus
                        onPicked: index => body.pickMarker(index)
                    }
                }

                MarkdownStyler {
                    id: styler
                    objectName: "styler"

                    sourceMode: Settings.sourceMode
                    focusMode: Settings.focusMode
                    foldComments: margin.room
                    spellcheck: Settings.spellcheck && !root.dataFile
                    // The note's own language, else the vault's, else the desktop's.
                    language: styler.frontMatter.lang || (Vault.isOpen ? Vault.vaultSetting(
                                                                             "language") : "")
                    fontFamily: root.noteFamily
                    cursorPosition: body.activeFocus ? body.cursorPosition : -1
                    pixelSize: body.font.pixelSize
                    width: body.width
                }

                Repeater {
                    model: styler.images.filter(each => !each.audio)

                    Image {
                        required property var modelData

                        readonly property rect line: {
                            body.layoutTick
                            // An edit can shorten the note before the list catches up.
                            return body.positionToRectangle(Math.min(modelData.position,
                                                                     body.length))
                        }

                        // Math within a line sits where its room is, in the middle of the line.
                        x: modelData.inline ? line.x : 0
                        y: modelData.inline ? line.y + (line.height - height) / 2 : line.y
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

                // A sound embedded on a line of its own plays there.
                Repeater {
                    id: players

                    /// The sounds, a fresh list after every edit. The players are counted from it
                    /// rather than made from it, so typing elsewhere leaves one playing as it is.
                    readonly property var sounds: styler.images.filter(each => each.audio)

                    model: sounds.length

                    AudioPlayer {
                        required property int index
                        readonly property var sound: players.sounds[index] ?? ({})

                        readonly property rect line: {
                            body.layoutTick
                            return body.positionToRectangle(Math.min(sound.position ?? 0,
                                                                     body.length))
                        }

                        objectName: "audioPlayer"
                        y: line.y
                        width: sound.width ?? 0
                        height: sound.height ?? 0
                        source: sound.source ?? ""
                    }
                }

                // CriticMarkup comments as notes in the margin beside their lines, where the
                // window has room for them; otherwise they stay in the text.
                Item {
                    id: margin

                    readonly property real space: flick.width - (page.x + page.width)
                    readonly property bool room: space >= 180 && !root.dataFile
                    readonly property real noteWidth: Math.min(240, space - Theme.space4)
                    /// Each comment with where it goes, below the one before when they would meet.
                    readonly property var placed: {
                        body.layoutTick
                        if (!room)
                            return []
                        const out = []
                        let below = -Infinity
                        for (const comment of styler.comments) {
                            const line = body.positionToRectangle(Math.min(comment.position,
                                                                           body.length))
                            const lines = Math.min(3, Math.max(1, Math.ceil(noteMetrics.advanceWidth(
                                                                                comment.text) / (
                                                                                noteWidth - 2
                                                                                * Theme.space2))))
                            const height = lines * noteMetrics.height + 2 * Theme.space2
                            const y = Math.max(line.y, below + Theme.space2)
                            out.push({
                                         "y": y,
                                         "height": height,
                                         "text": comment.text,
                                         "position": comment.position
                                     })
                            below = y + height
                        }
                        return out
                    }

                    x: body.width + Theme.space7 + Theme.space3
                    visible: room

                    FontMetrics {
                        id: noteMetrics
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }

                    Repeater {
                        model: margin.placed

                        Rectangle {
                            id: note

                            required property var modelData

                            objectName: "marginNote"
                            y: modelData.y
                            width: margin.noteWidth
                            height: modelData.height
                            radius: Theme.radiusMd
                            color: noteHover.hovered ? Theme.surfaceAlt : Theme.surface
                            border.color: Theme.border
                            Accessible.role: Accessible.Note
                            Accessible.name: modelData.text

                            Text {
                                anchors.fill: parent
                                anchors.margins: Theme.space2
                                wrapMode: Text.Wrap
                                maximumLineCount: 3
                                elide: Text.ElideRight
                                textFormat: Text.PlainText
                                text: note.modelData.text
                                color: Theme.textMuted
                                font: noteMetrics.font
                            }
                            HoverHandler {
                                id: noteHover
                                cursorShape: Qt.PointingHandCursor
                            }
                            Tip {
                                visible: noteHover.hovered
                                text: note.modelData.text
                            }
                            // To the comment, shown in its line to edit.
                            TapHandler {
                                onTapped: {
                                    body.forceActiveFocus()
                                    body.cursorPosition = note.modelData.position + 3
                                }
                            }
                        }
                    }
                }

                // Each table of contents, in the room its marker line keeps.
                Repeater {
                    model: styler.contents

                    Column {
                        id: contents

                        required property var modelData

                        readonly property rect line: {
                            body.layoutTick
                            return body.positionToRectangle(Math.min(modelData.position,
                                                                     body.length))
                        }

                        objectName: "contents"
                        y: line.y
                        width: body.width

                        Text {
                            visible: contents.modelData.headings.length === 0
                            height: contents.modelData.rowHeight
                            verticalAlignment: Text.AlignVCenter
                            text: qsTr("The note's headings will be listed here.")
                            color: Theme.textFaint
                            font.family: root.noteFamily
                            font.pixelSize: body.font.pixelSize
                        }
                        Repeater {
                            model: contents.modelData.headings

                            // The whole row takes the click, its indent too.
                            Item {
                                id: entry

                                required property var modelData

                                objectName: "contentsEntry"
                                width: contents.width
                                height: contents.modelData.rowHeight
                                Accessible.role: Accessible.Link
                                Accessible.name: modelData.text

                                Text {
                                    anchors.fill: parent
                                    leftPadding: (entry.modelData.level - 1) * Theme.space5
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                    text: entry.modelData.text
                                    color: entryHover.hovered ? Theme.accent : Theme.text
                                    font.family: root.noteFamily
                                    font.pixelSize: body.font.pixelSize
                                    font.underline: entryHover.hovered
                                }
                                HoverHandler {
                                    id: entryHover
                                    cursorShape: Qt.PointingHandCursor
                                }
                                TapHandler {
                                    // Holds the press, so the editor under it moves nothing.
                                    gesturePolicy: TapHandler.ReleaseWithinBounds
                                    onTapped: {
                                        body.forceActiveFocus()
                                        body.cursorPosition = entry.modelData.position
                                    }
                                }
                            }
                        }
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

                // A misspelled word's right click offers what it may have meant.
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: point => {
                        const at = body.positionAt(point.position.x, point.position.y)
                        const word = styler.misspelledAt(at)
                        if (word.word === undefined)
                            return
                        spellMenu.word = word
                        spellMenu.popup(body, point.position.x, point.position.y)
                    }
                }
                MenuCard {
                    id: spellMenu

                    property var word: ({})

                    objectName: "spellMenu"
                    minimumWidth: 180

                    Repeater {
                        model: spellMenu.word.suggestions || []

                        MenuEntry {
                            required property string modelData
                            text: modelData
                            // A note Jot only reads stays as it is.
                            enabled: !body.readOnly
                            onTriggered: {
                                styler.replace(spellMenu.word.start, spellMenu.word.end, modelData)
                                body.cursorPosition = spellMenu.word.start + modelData.length
                            }
                        }
                    }
                    MenuEntry {
                        visible: (spellMenu.word.suggestions || []).length === 0
                        height: visible ? implicitHeight : 0
                        enabled: false
                        text: qsTr("No suggestions")
                    }
                    MenuEntry {
                        text: qsTr("Add \"%1\" to the dictionary").arg(spellMenu.word.word)
                        onTriggered: styler.acceptWord(spellMenu.word.word, false)
                    }
                    MenuEntry {
                        text: qsTr("Ignore it until Jot closes")
                        onTriggered: styler.acceptWord(spellMenu.word.word, true)
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
                        if (done) {
                            body.remove(hit.position, hit.position + 1)
                            body.insert(hit.position, " ")
                            return
                        }
                        // Ticked, the whole line is written again, with the next one above it
                        // when it repeats.
                        const edit = Vault.tickedLineAt(body.text, hit.position)
                        styler.replace(edit.start, edit.end, edit.text)
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

    // While recording: for how long, and a way to stop.
    Rectangle {
        objectName: "recording"
        anchors {
            horizontalCenter: flick.horizontalCenter
            bottom: wordCount.visible ? wordCount.top : statusLine.visible ? statusLine.top :
                                                                             parent.bottom
            bottomMargin: Theme.space3
        }
        visible: recorder.recording || recordingProblem.text !== ""
        width: recordingRow.implicitWidth + 2 * Theme.space3
        height: recordingRow.implicitHeight + 2 * Theme.space2
        radius: Theme.radiusLg
        color: Theme.surface
        border.color: recorder.recording ? Theme.danger : Theme.border

        Row {
            id: recordingRow

            anchors.centerIn: parent
            spacing: Theme.space3

            Text {
                id: recordingProblem

                anchors.verticalCenter: parent.verticalCenter
                visible: text !== ""
                color: Theme.danger
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSm
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: recorder.recording
                text: qsTr("Recording %1").arg(Math.floor(recorder.duration / 60000) + ":" + String(Math.floor(
                                                                                                        recorder.duration
                                                                                                        / 1000) % 60).padStart(
                                                   2, "0"))
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSm
                font.features: {
                    "tnum": 1
                }
            }
            StickerButton {
                anchors.verticalCenter: parent.verticalCenter
                text: recorder.recording ? qsTr("Stop") : qsTr("OK")
                onClicked: recorder.recording ? recorder.stop() : recordingProblem.text = ""
            }
        }
    }

    // How long the note is, or what is picked of it, against its target and the day's goal.
    Text {
        id: wordCount

        readonly property var count: styler.count
        readonly property int picked: body.activeFocus && body.selectedText !== "" ? styler.countOf(
                                                                                         body.selectedText).words :
                                                                                     -1
        readonly property int target: Number(styler.frontMatter.target) || 0

        objectName: "wordCount"
        anchors {
            left: parent.left
            right: outline.visible && !outline.floating ? outline.left : parent.right
            bottom: statusLine.visible ? statusLine.top : parent.bottom
        }
        visible: Vault.current !== "" && !root.dataFile
        topPadding: Theme.space1
        bottomPadding: Theme.space1
        leftPadding: Theme.space4
        rightPadding: Theme.space4
        horizontalAlignment: Text.AlignRight
        elide: Text.ElideLeft
        color: Theme.textFaint
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textXs
        font.features: {
            "tnum": 1
        }
        text: {
            const words = count.words.toLocaleString(Qt.locale(), "f", 0)
            const parts = [picked >= 0 ? qsTr("%1 of %2 words").arg(picked.toLocaleString(Qt.locale(),
                                                                                          "f", 0)).arg(
                                             words) : count.words === 1 ? qsTr("1 word") : qsTr(
                                                                              "%1 words").arg(
                                                                              words)]
            parts.push(qsTr("%1 characters").arg(count.characters.toLocaleString(Qt.locale(), "f",
                                                                                 0)))

            if (target > 0)
                parts.push(qsTr("%1% of %2").arg(Math.floor(100 * count.words / target)).arg(
                               target.toLocaleString(Qt.locale(), "f", 0)))
            // The notebook's target, counting this note as it is now.
            if (Vault.currentTarget.words > 0)
                parts.push(Vault.targetProgress(count.words))
            const today = Vault.wordsToday.toLocaleString(Qt.locale(), "f", 0)
            parts.push(Settings.dailyWordGoal > 0 ? qsTr("today %1 of %2").arg(today).arg(Settings.dailyWordGoal.toLocaleString(
                                                                                              Qt.locale(
                                                                                                  ), "f", 0)) :
                                                    qsTr("today %1").arg(today))
            if (Vault.streak > 1)
                parts.push(qsTr("%1 days in a row").arg(Vault.streak))
            return parts.join(", ")
        }
    }

    FindBar {
        id: findBar

        anchors {
            right: flick.right
            top: flick.top
            margins: Theme.space4
        }
        z: 2
        styler: styler
        body: body
        writable: Vault.writable

        // Another note, or none, ends the search.
        Connections {
            target: Vault
            function onCurrentChanged() {
                if (findBar.visible)
                    findBar.close()
            }
        }
    }

    // Over the note while the outline lies on it: a click there puts it away.
    Scrim {
        objectName: "outlineBackdrop"
        z: 2
        visible: outline.visible && outline.floating
        anchors {
            left: flick.left
            right: outline.left
            top: flick.top
            bottom: flick.bottom
        }

        MouseArea {
            anchors.fill: parent
            onClicked: Settings.outlineShown = false
        }
    }
    Outline {
        id: outline

        /// Without room for the note beside it, it lies over the note's edge instead.
        readonly property bool floating: root.width < implicitWidth + Theme.noteRoomWidth
        /// It took the keys from the note when it opened over it, to give them back.
        property bool holding: false

        objectName: "outline"
        z: floating ? 3 : 0
        // Lying over the note it takes the keys, so Escape puts it away without the editor's
        // own Escape seeing it, and however it is put away gives them back to the note.
        onVisibleChanged: {
            if (visible && floating) {
                holding = true
                forceActiveFocus()
            } else if (!visible && holding) {
                holding = false
                body.forceActiveFocus()
            }
        }
        Keys.onEscapePressed: Settings.outlineShown = false
        anchors {
            right: parent.right
            top: flick.top
            bottom: flick.bottom
        }
        visible: Settings.outlineShown && Vault.current !== "" && !root.dataFile
        width: Math.min(implicitWidth, root.width - 2 * Theme.space7)
        // Read only while shown, as it is the whole note again after each edit.
        headings: visible ? styler.headings : []
        current: Settings.readMode || !body.activeFocus ? -1 : styler.headingAt(headings,
                                                                                body.cursorPosition)
        onChosen: index => {
            root.goToHeading(index)
            // Lying over the note, it gets out of the way of the heading it went to.
            if (outline.floating)
                Settings.outlineShown = false
        }

        // Its own ground, so the note does not show through where it lies over it.
        Rectangle {
            z: -1
            visible: outline.floating
            anchors.fill: parent
            color: Theme.bg

            SoftShadow {
                anchors.fill: parent
            }
        }
    }

    ViStatusLine {
        id: statusLine

        objectName: "viStatusLine"
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        // Vi works in the text, so a table shown in its place has no use for the line.
        visible: Settings.viMode && Vault.current !== "" && !root.tableShown
        mode: vi.mode
        pending: vi.pending
        path: Vault.current
        kind: root.tableShown ? "table" : root.dataFile ? "data" : "note"
        showCursor: body.visible
        line: vi.line
        column: vi.column
        pickedLines: vi.pickedLines
        place: vi.percent + "%"
    }

    ConflictResolver {
        id: resolver
    }

    TaskSchedule {
        id: scheduler

        /// Where the to-do's words are in the note, to put the new ones in their place.
        property var place

        onChosen: line => {
            styler.replace(place.start, place.end, line)
            body.forceActiveFocus()
        }
        onClosed: body.forceActiveFocus()
    }

    TrashConfirm {
        id: trashConfirm
    }
}
