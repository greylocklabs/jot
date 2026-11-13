pragma ComponentBehavior: Bound
import Jot.Ui

import QtQuick
import QtQuick.Controls
import QtQuick.Effects

/// The main window: a frameless frame with Jot's own title bar, as Callie draws it.
ApplicationWindow {
    id: window

    /// `words`, and the shortcut for `id` after them when it has one, as chosen in Settings.
    function withKey(words, id) {
        keys.actions
        const key = keys.keyText(id)
        return key === "" ? words : qsTr("%1 (%2)").arg(words).arg(key)
    }

    width: 1280
    height: 840
    minimumWidth: 600
    minimumHeight: 560
    visible: true
    title: qsTr("Jot")

    /// Fullscreen with only the note: no title bar, sidebar, list or format row.
    property bool distractionFree: false
    /// How the window was before going distraction free, to go back to.
    property int visibilityBefore: Window.Windowed

    function setDistractionFree(on) {
        if (on === distractionFree)
            return
        if (on) {
            visibilityBefore = visibility
            distractionFree = true
            showFullScreen()
        } else {
            distractionFree = false
            if (visibilityBefore === Window.Maximized)
                showMaximized()
            else
                showNormal()
        }
    }
    // Leaving fullscreen some other way, as from the desktop, brings the rest back.
    onVisibilityChanged: {
        if (distractionFree && visibility !== Window.FullScreen)
            distractionFree = false
    }
    // Transparent where the rounded frame leaves its corners.
    color: frame.rounded ? "transparent" : Theme.bg
    flags: Qt.Window | Qt.FramelessWindowHint

    // Everything the window shows, with its corners rounded unless it fills
    // the screen.
    Item {
        id: frame

        // The software renderer has no shader effects, so it keeps square corners.
        readonly property bool rounded: window.visibility === Window.Windowed && GraphicsInfo.api
                                        !== GraphicsInfo.Software

        anchors.fill: parent
        layer.enabled: rounded
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: frameMask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1
        }

        Rectangle {
            anchors.fill: parent
            color: Theme.bg
        }

        Rectangle {
            id: titleBar

            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
            }
            visible: !window.distractionFree
            height: visible ? Theme.titleBarHeight : 0
            color: Theme.surface

            // Behind the controls: dragging the bar moves the window, double-clicking
            // maximizes it, as a desktop title bar would.
            DragHandler {
                target: null
                onActiveChanged: {
                    if (active)
                        window.startSystemMove()
                }
            }
            TapHandler {
                onDoubleTapped: window.visibility === Window.Maximized ? window.showNormal() :
                                                                         window.showMaximized()
            }

            Rectangle {
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                }
                height: 1
                color: Theme.border
            }

            Row {
                id: leading

                anchors {
                    left: parent.left
                    verticalCenter: parent.verticalCenter
                    leftMargin: WindowButtons.left.length > 0 ? Theme.space4 : Theme.space5
                }
                spacing: Theme.space4

                WindowControls {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: buttons.length > 0
                    buttons: WindowButtons.left
                }
                // The notebooks and the note list, shown or put away.
                StickerButton {
                    objectName: "columnsToggle"
                    anchors.verticalCenter: parent.verticalCenter
                    visible: Vault.isOpen
                    glyph: "sidebar"
                    Accessible.name: columns.showSidebar || columns.showList ? qsTr(
                                                                                   "Put away the notebooks and notes") :
                                                                               qsTr("Show the notebooks and notes")
                    tip: Accessible.name + " (Ctrl+\\)"
                    onClicked: window.run("columns")
                }
                Logo {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: -Theme.logoLift
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: window.title
                    color: Theme.text
                    font.family: Theme.displayFontFamily
                    font.pixelSize: Theme.textDisplay
                    font.weight: Theme.weightBold
                }
                // The notes folder's name, cut short before it reaches the buttons on the right.
                Text {
                    objectName: "vaultName"
                    anchors.verticalCenter: parent.verticalCenter
                    visible: Vault.isOpen
                    width: Math.max(0, Math.min(implicitWidth, trailing.x - leading.x - x
                                                - travel.width - 2 * leading.spacing
                                                - Theme.space4))
                    text: Vault.name
                    elide: Text.ElideRight
                    color: Theme.textMuted
                    font.family: Theme.displayFontFamily
                    font.pixelSize: Theme.textDisplay
                    font.weight: Theme.weightStrong
                }

                // Back and forward through the notes opened, as a browser's arrows.
                Row {
                    id: travel

                    anchors.verticalCenter: parent.verticalCenter
                    visible: Vault.isOpen
                    spacing: Theme.space2

                    StickerButton {
                        id: backButton

                        objectName: "goBack"
                        enabled: Vault.canGoBack
                        glyph: "chevron-left"
                        Accessible.name: qsTr("Back")
                        tip: qsTr("%1; hold for more").arg(window.withKey(qsTr(
                                                                              "Back to the note before"),
                                                                          "goBack"))
                        onClicked: Vault.goBack()
                        onPressAndHold: trailMenu.show(backButton, false)

                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: trailMenu.show(backButton, false)
                        }
                    }
                    StickerButton {
                        id: forwardButton

                        objectName: "goForward"
                        enabled: Vault.canGoForward
                        glyph: "chevron-right"
                        Accessible.name: qsTr("Forward")
                        tip: qsTr("%1; hold for more").arg(window.withKey(qsTr(
                                                                              "Forward to the note after"),
                                                                          "goForward"))
                        onClicked: Vault.goForward()
                        onPressAndHold: trailMenu.show(forwardButton, true)

                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: trailMenu.show(forwardButton, true)
                        }
                    }

                    // The notes back or forward, nearest first, to jump straight to one.
                    MenuCard {
                        id: trailMenu

                        property bool forward: false
                        property var entries: []

                        function show(button, forward) {
                            trailMenu.forward = forward
                            entries = forward ? Vault.trailAhead() : Vault.trailBehind()
                            if (entries.length > 0)
                                popup(button, 0, button.height + Theme.space1)
                        }

                        objectName: "trailMenu"

                        Instantiator {
                            model: trailMenu.entries

                            MenuEntry {
                                required property var modelData
                                required property int index

                                text: modelData.title
                                onTriggered: Vault.goBy(trailMenu.forward ? index + 1 : -(index
                                                                                          + 1))

                            }
                            onObjectAdded: (index, object) => trailMenu.insertItem(index, object)
                            onObjectRemoved: (index, object) => trailMenu.removeItem(object)
                        }
                    }
                }
            }

            Row {
                id: trailing

                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    rightMargin: Theme.space4
                }
                spacing: Theme.space4

                // When the notes last synced, red after a sync that failed, with a dot while
                // changes wait for the next. On a narrow window it goes into the button's tip.
                Row {
                    id: syncStamp

                    objectName: "syncStamp"
                    anchors.verticalCenter: parent.verticalCenter
                    visible: syncButton.visible && window.width >= 960 && (Vault.sync.stamp !== ""
                                                                           || Vault.sync.problem
                                                                           !== "")
                    spacing: Theme.space2

                    Rectangle {
                        objectName: "syncPending"
                        anchors.verticalCenter: parent.verticalCenter
                        visible: Vault.sync.pending && !Vault.sync.syncing
                        width: Theme.space2
                        height: width
                        radius: width / 2
                        color: Theme.accent
                    }
                    Text {
                        objectName: "syncStampText"
                        anchors.verticalCenter: parent.verticalCenter
                        text: Vault.sync.stamp !== "" ? Vault.sync.stamp : qsTr("Not synced")
                        color: Vault.sync.problem !== "" ? Theme.danger : Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                        font.weight: Theme.weightStrong
                    }
                    HoverHandler {
                        id: stampHover
                    }
                    Loader {
                        active: stampHover.hovered && Vault.sync.details !== ""

                        sourceComponent: Tip {
                            parent: syncStamp
                            text: Vault.sync.details
                            delay: Theme.durSlow * 2
                            closePolicy: Popup.NoAutoClose
                        }
                    }
                }

                // Where the notes sync to: how it stands, and a sync at a click.
                StickerButton {
                    id: syncButton

                    objectName: "syncButton"
                    anchors.verticalCenter: parent.verticalCenter
                    visible: Vault.isOpen && Vault.sync.configured
                    glyph: "refresh"
                    spinning: Vault.sync.syncing
                    Accessible.name: qsTr("Sync now")
                    tip: syncStamp.visible || Vault.sync.details === "" ? Vault.sync.summary :
                                                                          Vault.sync.summary + "\n"
                                                                          + Vault.sync.details
                    onClicked: Vault.sync.syncNow()

                    // The last sync went wrong; the tip says how.
                    Rectangle {
                        visible: Vault.sync.problem !== "" && !Vault.sync.syncing
                        width: Theme.badgeSize
                        height: Theme.badgeSize
                        x: parent.width - width * 0.8
                        y: -height * 0.2
                        radius: width / 2
                        color: Theme.danger
                        border.width: width / 5
                        border.color: Theme.bg
                    }
                }

                // Always a note; its extension can change after. Not in Trash, where it would be
                // unclear where the note goes.
                WhyDisabled {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: Vault.isOpen && Vault.writable
                    target: newButton
                    reason: qsTr("Not in Trash, as there is no notebook for a new note to go in.")

                    StickerButton {
                        id: newButton

                        enabled: !Vault.showingTrash
                        objectName: "newButton"
                        glyph: "plus"
                        Accessible.name: qsTr("New note")
                        tip: window.withKey(qsTr("New note"), "newNote")
                        onClicked: window.run("newNote")
                    }
                }

                StickerButton {
                    id: searchButton

                    anchors.verticalCenter: parent.verticalCenter
                    visible: Vault.isOpen
                    glyph: "search"
                    Accessible.name: qsTr("Go to a note")
                    tip: window.withKey(qsTr("Go to a note"), "goTo")
                    onClicked: switcher.open()
                }

                StickerButton {
                    id: settingsButton

                    anchors.verticalCenter: parent.verticalCenter
                    glyph: "settings"
                    glyphStroke: Theme.fineGlyphStroke
                    Accessible.name: qsTr("Settings")
                    tip: window.withKey(qsTr("Settings"), "settings")
                    onClicked: settingsDialog.open()
                }

                // Help: debug info, logs and bug reports.
                StickerButton {
                    id: helpButton

                    anchors.verticalCenter: parent.verticalCenter
                    text: "?"
                    Accessible.name: qsTr("Help")
                    // Not beside its own open menu, which leaves the button hovered.
                    tip: helpMenu.visible ? "" : qsTr("Help, shortcuts and about")
                    onClicked: helpMenu.popup(helpButton, 0, helpButton.height + Theme.space3)

                    Tip {
                        visible: copiedTip.running
                        text: qsTr("Debug info copied")
                    }

                    // A newer release is out; the menu says which. It sits on the corner,
                    // a fifth of it past the edge, ringed in the background color.
                    Rectangle {
                        visible: Release.update !== ""
                        width: Theme.badgeSize
                        height: Theme.badgeSize
                        x: parent.width - width * 0.8
                        y: -height * 0.2
                        radius: width / 2
                        color: Theme.accent
                        border.width: width / 5
                        border.color: Theme.bg
                    }

                    Timer {
                        id: copiedTip
                        interval: 2000
                    }

                    Connections {
                        target: Support
                        function onCopied() {
                            copiedTip.restart()
                        }
                    }

                    MenuCard {
                        id: helpMenu
                        minimumWidth: 200

                        MenuEntry {
                            text: qsTr("Update to Jot %1...").arg(Release.update)
                            visible: Release.update !== ""
                            height: visible ? implicitHeight : 0
                            onTriggered: updateDialog.open()
                        }
                        MenuEntry {
                            text: qsTr("Copy debug info")
                            onTriggered: Support.copyDebugInfo()
                        }
                        MenuEntry {
                            text: qsTr("Open logs folder")
                            onTriggered: Support.openLogs()
                        }
                        MenuEntry {
                            text: qsTr("Report a problem...")
                            onTriggered: reportDialog.open()
                        }
                        MenuSeparator {
                            contentItem: Rectangle {
                                implicitHeight: 1
                                color: Theme.border
                            }
                        }
                        MenuEntry {
                            text: qsTr("Keyboard shortcuts")
                            onTriggered: keyHelp.open()
                        }
                        MenuEntry {
                            text: qsTr("Syntax guide")
                            onTriggered: syntaxGuide.open()
                        }
                        MenuEntry {
                            text: qsTr("What's new")
                            visible: Release.notes !== "" || Release.breaking !== ""
                            height: visible ? implicitHeight : 0
                            onTriggered: whatsNew.open()
                        }
                        MenuEntry {
                            text: qsTr("About Jot")
                            onTriggered: about.open()
                        }
                        MenuEntry {
                            text: qsTr("Support Jot on Ko-fi")
                            onTriggered: Qt.openUrlExternally(Release.support)
                        }
                    }
                }

                WindowControls {
                    id: rightControls

                    anchors.verticalCenter: parent.verticalCenter
                    visible: buttons.length > 0
                    buttons: WindowButtons.right
                }
            }
        }

        Item {
            id: columns

            // The notebooks and the note list go together as the window narrows, so the note
            // keeps room to write in, and come back together as it widens. Put away by hand they
            // stay away; brought back by hand on a narrow window, they squeeze the note until the
            // window is wide enough for them again.
            readonly property int wantedSidebar: Settings.sidebarWidth > 0 ? Settings.sidebarWidth :
                                                                             Theme.sidebarWidth
            readonly property int wantedList: Settings.noteListWidth > 0 ? Settings.noteListWidth :
                                                                           Theme.noteListWidth
            /// The note keeps its room beside both, at the widths they are set to.
            readonly property bool fits: width - wantedSidebar - wantedList >= Theme.noteRoomWidth
            /// Brought back by hand while the window is too narrow for them.
            property bool shownByHand: false
            readonly property bool shown: !window.distractionFree && (shownByHand || (
                                                                          !Settings.columnsHidden
                                                                          && fits))
            readonly property bool showSidebar: shown
            readonly property bool showList: shown
            /// Brought back on a window too narrow for them, they lie over the note, to pick from
            /// and put away again, rather than squeezing it.
            readonly property bool flyout: shown && !fits
            // Over the note, a strip of it stays in view beside them.
            readonly property int room: flyout ? width - 2 * Theme.space7 : width
                                                 - Theme.noteRoomWidth

            readonly property int sidebarWidth: shown ? Math.max(0, Math.min(wantedSidebar, room
                                                                             - Settings.minNoteListWidth)) :
                                                        0
            readonly property int noteListWidth: shown ? Math.max(0, Math.min(wantedList, room
                                                                              - sidebarWidth)) : 0

            /// Puts both columns away, or brings both back.
            function toggle() {
                if (shown) {
                    shownByHand = false
                    Settings.columnsHidden = true
                } else {
                    Settings.columnsHidden = false
                    shownByHand = !fits
                }
            }

            /// Puts the columns away again once the note under them is wanted.
            function closeFlyout() {
                if (flyout)
                    shownByHand = false
            }

            /// Brings the columns back, if they are away, for something that needs the list.
            function revealList() {
                if (shown)
                    return
                Settings.columnsHidden = false
                if (!fits)
                    shownByHand = true
            }

            onFitsChanged: {
                if (fits)
                    shownByHand = false
            }

            anchors {
                left: parent.left
                right: parent.right
                top: titleBar.bottom
                bottom: parent.bottom
            }

            Welcome {
                anchors.fill: parent
                visible: !Vault.isOpen
            }

            Sidebar {
                id: sidebar

                z: columns.flyout ? 3 : 0
                objectName: "sidebar"
                onNoteMade: editor.editTitle()
                onJoplinImportRequested: folder => joplinImport.start(folder)
                onEntered: window.focusShownList()
                visible: Vault.isOpen && columns.showSidebar
                anchors {
                    left: parent.left
                    top: parent.top
                    bottom: parent.bottom
                }
                width: columns.sidebarWidth
            }
            ColumnSplitter {
                z: columns.flyout ? 3 : 0
                visible: sidebar.visible
                anchors {
                    horizontalCenter: sidebar.right
                    top: parent.top
                    bottom: parent.bottom
                }
                columnWidth: sidebar.width
                // No wider than leaves the note its room beside both, so a drag never puts
                // them away; brought back by hand on a narrow window, as wide as dragged.
                onResized: width => Settings.sidebarWidth = columns.shownByHand ? width : Math.min(
                                                                                      width, columns.width
                                                                                      - columns.wantedList
                                                                                      - Theme.noteRoomWidth)
                onReset: Settings.resetSidebarWidth()
            }
            TaskList {
                id: taskList

                z: columns.flyout ? 3 : 0
                onLeftList: sidebar.focusRows()
                onAddRequested: quickAdd.open()
                onTaskPicked: closeAfterPick.restart()
                visible: Vault.isOpen && Vault.showingTasks && columns.showList
                anchors {
                    left: sidebar.right
                    top: parent.top
                    bottom: parent.bottom
                }
                width: columns.noteListWidth
            }
            NoteList {
                id: noteList

                onLeftList: sidebar.focusRows()

                z: columns.flyout ? 3 : 0
                objectName: "noteList"
                onTitleEditRequested: editor.editTitle()
                onHistoryRequested: window.run("history")
                onExportRequested: path => window.exportNote(path)
                // Once it is clear the click is no double-click to rename the note in its row.
                onNotePicked: closeAfterPick.restart()

                Timer {
                    id: closeAfterPick
                    interval: Application.styleHints.mouseDoubleClickInterval
                    onTriggered: {
                        if (noteList.renamingPath === "")
                            columns.closeFlyout()
                    }
                }
                visible: Vault.isOpen && !Vault.showingTasks && !Vault.showingTrash
                         && Vault.shownTag === "" && columns.showList

                anchors {
                    left: sidebar.right
                    top: parent.top
                    bottom: parent.bottom
                }
                width: columns.noteListWidth
            }
            ColumnSplitter {
                objectName: "listSplitter"
                z: columns.flyout ? 3 : 0
                visible: Vault.isOpen && columns.showList
                anchors {
                    horizontalCenter: noteList.right
                    top: parent.top
                    bottom: parent.bottom
                }
                columnWidth: noteList.width
                onResized: width => Settings.noteListWidth = columns.shownByHand ? width : Math.min(
                                                                                       width, columns.width
                                                                                       - columns.wantedSidebar
                                                                                       - Theme.noteRoomWidth)
                onReset: Settings.resetNoteListWidth()
            }
            TagView {
                id: tagView

                objectName: "tagView"
                onLeftList: sidebar.focusRows()
                z: columns.flyout ? 3 : 0
                visible: Vault.isOpen && Vault.shownTag !== "" && columns.showList
                onPicked: closeAfterPick.restart()
                anchors {
                    left: sidebar.right
                    top: parent.top
                    bottom: parent.bottom
                }
                width: columns.noteListWidth
            }
            TrashList {
                id: trashList

                onLeftList: sidebar.focusRows()

                z: columns.flyout ? 3 : 0
                visible: Vault.isOpen && Vault.showingTrash && columns.showList
                onNotePicked: columns.closeFlyout()
                anchors {
                    left: sidebar.right
                    top: parent.top
                    bottom: parent.bottom
                }
                width: columns.noteListWidth
            }
            TrashPreview {
                location: trashList.chosen
                visible: Vault.isOpen && Vault.showingTrash
                anchors {
                    left: columns.flyout ? parent.left : trashList.right
                    right: parent.right
                    top: parent.top
                    bottom: parent.bottom
                }
            }
            // Over the note while the columns lie on it: a click there puts them away.
            Scrim {
                objectName: "columnsBackdrop"
                z: 2
                visible: columns.flyout && Vault.isOpen
                anchors {
                    left: parent.left
                    leftMargin: columns.sidebarWidth + columns.noteListWidth
                    right: parent.right
                    top: parent.top
                    bottom: parent.bottom
                }
                radius: 0

                MouseArea {
                    anchors.fill: parent
                    onClicked: columns.closeFlyout()
                }
            }

            NoteEditor {
                id: editor

                router: keys
                onLeftNote: {
                    columns.revealList()
                    noteList.focusList()
                }
                distractionFree: window.distractionFree

                visible: Vault.isOpen && !Vault.showingTrash
                anchors {
                    left: columns.flyout ? parent.left : noteList.right
                    right: parent.right
                    top: parent.top
                    bottom: parent.bottom
                }
            }
        }
    }

    CommitDialog {
        id: commitDialog
    }

    QuickSwitcher {
        id: switcher
        objectName: "switcher"
    }

    QuickAdd {
        id: quickAdd
    }

    CommandPalette {
        id: palette
        router: keys
        onChosen: id => window.run(id)
    }

    JoplinImport {
        id: joplinImport
    }

    LazyPopup {
        id: settingsDialog
        sourceComponent: SettingsDialog {
            onEditColorsRequested: {
                close()
                themeEditor.open()
            }
            onJoplinImportRequested: {
                close()
                joplinImport.start("")
            }
        }
    }
    LazyPopup {
        id: themeEditor
        sourceComponent: ThemeEditor {
            onFinished: settingsDialog.open()
        }
    }
    LazyPopup {
        id: keyHelp
        sourceComponent: KeyHelp {
            actions: keys.actions
        }
    }
    LazyPopup {
        id: syntaxGuide
        sourceComponent: SyntaxGuide {}
    }
    LazyPopup {
        id: about
        sourceComponent: AboutDialog {
            onWhatsNewRequested: whatsNew.open()
        }
    }
    LazyPopup {
        id: whatsNew
        sourceComponent: WhatsNewDialog {}
    }
    LazyPopup {
        id: updateDialog
        sourceComponent: UpdateDialog {}
    }
    LazyPopup {
        id: exportDialog
        sourceComponent: ExportDialog {}
    }
    LazyPopup {
        id: history
        sourceComponent: HistoryDialog {}
    }
    LazyPopup {
        id: reportDialog
        sourceComponent: ReportDialog {}
    }
    // The vault's templates, for a new note made from one.
    MenuCard {
        id: templateMenu

        property var templates: []

        minimumWidth: 200

        Repeater {
            model: templateMenu.templates

            MenuEntry {
                required property var modelData
                text: modelData.title
                onTriggered: {
                    Vault.showingTasks = false
                    if (Vault.newNoteFromTemplate(modelData.path) !== "")
                        editor.editTitle()
                }
            }
        }
        MenuEntry {
            visible: templateMenu.templates.length === 0
            height: visible ? implicitHeight : 0
            enabled: false
            text: qsTr("No templates yet: notes in %1 are templates").arg(Vault.templatesFolder())
        }
    }
    ConfirmDialog {
        id: changeConfirm
        objectName: "changeConfirm"
    }
    // Saving is manual and the note shown has edits not yet written.
    ConfirmDialog {
        id: leaveConfirm
        objectName: "leaveConfirm"
    }
    Connections {
        target: Vault
        function onLeavingUnsaved() {
            leaveConfirm.destructive = false
            leaveConfirm.otherText = qsTr("Don't save")
            leaveConfirm.onOther = () => Vault.answerLeaving("discard")
            leaveConfirm.onDismiss = () => Vault.answerLeaving("stay")
            leaveConfirm.ask(qsTr("Save changes to %1?").arg(Vault.currentTitle), qsTr(
                                 "Without saving, what was typed since the last save is lost."),
                             qsTr("Save"), () => Vault.answerLeaving("save"))
        }
    }
    // Closing the window quits unless Jot keeps running, so it asks first too.
    onClosing: close => {
        if (!Settings.keepRunning && Vault.asksBeforeLeaving()) {
            close.accepted = false
            Vault.quit()
        }
    }

    // After an update, says what changed, once the window has settled.
    Timer {
        interval: 400
        running: true
        onTriggered: {
            if (Release.takeUpdateNotice())
                whatsNew.open()
        }
    }

    /// Asks how to export the note at `path`, and where.
    function exportNote(path) {
        exportDialog.active = true
        exportDialog.popup.path = path
        exportDialog.open()
    }

    // ---- Keyboard ----------------------------------------------------------------
    /// Gives the keyboard to the list shown: the to-dos, the trash or the notes.
    function focusShownList() {
        columns.revealList()
        if (Vault.shownTag !== "")
            tagView.focusList()
        else if (Vault.showingTasks)
            taskList.focusList()
        else if (Vault.showingTrash)
            trashList.focusList()
        else
            noteList.focusList()
    }

    /// Whether the command with this KeyRouter action id can run now.
    function can(id) {
        const note = Vault.current !== ""
        switch (id) {
        case "newNote":
        case "newFromTemplate":
            return Vault.isOpen && Vault.writable && !Vault.showingTrash
        case "dailyNote":
            return Vault.isOpen && Vault.writable
        case "quickAdd":
        case "importJoplin":
            return Vault.isOpen && Vault.writable
        case "goTo":
        case "search":
        case "find":
        case "save":
        case "tasks":
            return Vault.isOpen
        case "replace":
            return note && Vault.writable && !Vault.showingTrash
        case "gitCommit":
            return Vault.isOpen && Vault.writable && Vault.sync.manualCommits
        case "gitPush":
            return Vault.isOpen && Vault.sync.manualPushes && !Vault.sync.syncing
        case "columns":
            return Vault.isOpen && !window.distractionFree
        case "syncNow":
            return Vault.isOpen && Vault.sync.configured
        case "goBack":
            return Vault.canGoBack
        case "goForward":
            return Vault.canGoForward
        case "undoChange":
            return Vault.canUndo
        case "redoChange":
            return Vault.canRedo
        case "nextNote":
        case "previousNote":
        case "firstNote":
        case "lastNote":
            return Vault.isOpen
        case "closeTab":
            return Vault.tabs.length > 0
        case "nextTab":
        case "previousTab":
            return Vault.tabs.length > 1
        case "focusMode":
        case "typewriter":
        case "lineNumbers":
        case "distractionFree":
            return Vault.isOpen
        case "export":
            return note && Vault.currentKind === "markdown"
        case "record":
            return note && Vault.currentKind === "markdown" && Vault.writable && (editor.canRecord
                                                                                  || editor.recording)

        case "editNote":
        case "readMode":
        case "outline":
        case "copyLink":
        case "showInFiles":
        case "history":
            return note
        case "rename":
        case "toggleFavorite":
        case "hideNote":
            return note && Vault.writable
        case "trash":
            // Not while the tasks or the trash are shown, which hide the note it would ask about.
            return note && Vault.writable && !Vault.showingTasks && !Vault.showingTrash
        case "update":
            return Release.update !== ""
        case "whatsNew":
            return Release.notes !== "" || Release.breaking !== ""
        }
        return true
    }

    /// Does what the keyboard or the command palette asked for, by KeyRouter's action id.
    function run(id) {
        if (!can(id))
            return
        switch (id) {
        case "newNote":
            if (Vault.newNote() !== "")
                editor.editTitle()
            break
        case "newFromTemplate":
            templateMenu.templates = Vault.templates()
            templateMenu.popup(newButton, 0, newButton.height + Theme.space1)
            break
        case "dailyNote":
            Vault.showingTasks = false
            Vault.showingTrash = false
            Vault.openDailyNote()
            break
        case "goTo":
            switcher.open()
            break
        case "goBack":
            Vault.goBack()
            break
        case "syncNow":
            Vault.sync.syncNow()
            break
        case "quit":
            // With saving manual, asks about unsaved typing first.
            Vault.quit()
            break
        case "goForward":
            Vault.goForward()
            break
        case "undoChange":
            // Asks first when taking it back would move a note to the trash.
            if (Vault.undoTrashes !== "")
                changeConfirm.ask(qsTr("Move %1 to the trash again?").arg(Vault.undoTrashes), "",
                                  qsTr("Move to trash"), () => Vault.undo())
            else
                Vault.undo()
            break
        case "redoChange":
            if (Vault.redoTrashes !== "")
                changeConfirm.ask(qsTr("Move %1 to the trash again?").arg(Vault.redoTrashes), "",
                                  qsTr("Move to trash"), () => Vault.redo())
            else
                Vault.redo()
            break
        case "quickAdd":
            quickAdd.open()
            break
        case "commands":
            palette.unavailable = keys.actions.filter(a => a.scope === "window" && !can(a.id)).map(a
                                                                                                   => a.id)
            palette.open()
            break
        case "search":
            // Searching is in the notes, so it leaves a tag's view for them.
            Vault.dropTag()
            columns.revealList()
            noteList.focusSearch()
            break
        case "find":
            // With no note to find in, the vault is searched instead.
            if (Vault.current !== "" && !Vault.showingTrash)
                editor.find(false)
            else
                run("search")
            break
        case "replace":
            editor.find(true)
            break
        case "gitCommit":
            commitDialog.open()
            break
        case "gitPush":
            Vault.sync.push()
            break
        case "save":
            Vault.saveNow()
            break
        case "tasks":
            Vault.showingTasks = !Vault.showingTasks
            break
        case "settings":
            settingsDialog.open()
            break
        case "help":
            keyHelp.open()
            break
        case "nextNote":
        case "previousNote":
        case "firstNote":
        case "lastNote":
            // Through the sidebar while it has the keys, else the list shown.
            columns.revealList()
            if (sidebar.hasKeys())
                sidebar.step(id)
            else if (Vault.shownTag !== "")
                tagView.step(id)
            else if (Vault.showingTasks)
                taskList.step(id)
            else if (Vault.showingTrash)
                trashList.step(id)
            else
                noteList.step(id)
            break
        case "editNote":
            editor.write()
            break
        case "rename":
            // In the note's row, as a double-click there does, with the list shown for it.
            if (Vault.showingTasks || Vault.showingTrash || Vault.shownTag !== "") {
                editor.editTitle()
            } else {
                columns.revealList()
                noteList.rename(Vault.current)
            }
            break
        case "trash":
            noteList.askToTrash(Vault.current)
            break
        case "sourceMode":
            // Out of reading too, as the switch does, since the source is only seen writing.
            Settings.readMode = false
            Settings.sourceMode = !Settings.sourceMode
            break
        case "outline":
            Settings.outlineShown = !Settings.outlineShown
            break
        case "columns":
            columns.toggle()
            break
        case "export":
            exportNote(Vault.current)
            break
        case "record":
            editor.format("record", "")
            break
        case "focusMode":
            Settings.focusMode = !Settings.focusMode
            break
        case "typewriter":
            Settings.typewriter = !Settings.typewriter
            break
        case "lineNumbers":
            Settings.lineNumbers = !Settings.lineNumbers
            break
        case "distractionFree":
            window.setDistractionFree(!window.distractionFree)
            break
        case "closeTab":
            Vault.closeTab(Vault.tabIndex)
            break
        case "nextTab":
            Vault.stepTab(1)
            break
        case "previousTab":
            Vault.stepTab(-1)
            break
        case "readMode":
            if (Settings.readMode)
                editor.write()
            else
                Settings.readMode = true
            break
        case "copyLink":
            Support.copyText(Vault.linkTo(Vault.current))
            break
        case "showInFiles":
            Vault.showInFiles(Vault.current)
            break
        case "history":
            history.open()
            break
        case "toggleFavorite":
            Vault.setFavorite(Vault.current, !Vault.isFavorite(Vault.current))
            break
        case "hideNote":
            Vault.setHidden(Vault.current, true)
            break
        case "importJoplin":
            joplinImport.start("")
            break
        case "viMode":
            Settings.viMode = !Settings.viMode
            break
        case "editColors":
            themeEditor.open()
            break
        case "update":
            updateDialog.open()
            break
        case "whatsNew":
            whatsNew.open()
            break
        case "copyDebugInfo":
            Support.copyDebugInfo()
            break
        case "openLogs":
            Support.openLogs()
            break
        case "report":
            reportDialog.open()
            break
        case "about":
            about.open()
            break
        case "support":
            Qt.openUrlExternally(Release.support)
            break
        }
    }

    KeyRouter {
        id: keys
        window: window
        viMode: Settings.viMode
        leaderKey: Settings.leaderKey
        leaderTimeout: Settings.leaderTimeout
        onTriggered: id => window.run(id)
    }

    // The shortcuts that work anywhere. Those for the list and the note are their own.
    Instantiator {
        model: keys.actions.filter(a => a.scope === "window" && a.standard.length > 0)

        Shortcut {
            required property var modelData
            sequences: modelData.standard
            onActivated: window.run(modelData.id)
        }
    }

    // Dims the window while the leader's keys are shown, as a popup does.
    Scrim {
        anchors.fill: parent
        z: 899
        opacity: whichKey.shown ? 1 : 0
        visible: opacity > 0
    }
    WhichKey {
        id: whichKey
        router: keys
    }

    Rectangle {
        id: frameMask
        anchors.fill: frame
        radius: Theme.radiusMd
        visible: false
        layer.enabled: true
    }

    ResizeFrame {
        anchors.fill: parent
        z: 1000
    }
}
