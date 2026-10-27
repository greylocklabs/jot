pragma ComponentBehavior: Bound
import Jot.Ui

import QtQuick
import QtQuick.Controls
import QtQuick.Effects

/// The main window: a frameless frame with Jot's own title bar, as Callie draws it.
ApplicationWindow {
    id: window

    width: 1280
    height: 840
    minimumWidth: 600
    minimumHeight: 560
    visible: true
    title: qsTr("Jot")
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
            height: Theme.titleBarHeight
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
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: Vault.isOpen
                    text: Vault.name
                    color: Theme.textMuted
                    font.family: Theme.displayFontFamily
                    font.pixelSize: Theme.textDisplay
                    font.weight: Theme.weightStrong
                }
            }

            StickerButton {
                id: newButton

                anchors {
                    right: searchButton.left
                    rightMargin: Theme.space3
                    verticalCenter: parent.verticalCenter
                }
                visible: Vault.isOpen && Vault.writable
                glyph: "plus"
                Accessible.name: qsTr("New note")
                tip: qsTr("New note (Ctrl+N)")
                onClicked: {
                    if (Vault.newNote() !== "")
                        editor.editTitle()
                }
            }

            StickerButton {
                id: searchButton

                anchors {
                    right: settingsButton.left
                    rightMargin: Theme.space3
                    verticalCenter: parent.verticalCenter
                }
                visible: Vault.isOpen
                glyph: "search"
                Accessible.name: qsTr("Go to a note")
                tip: qsTr("Go to a note (Ctrl+P)")
                onClicked: switcher.open()
            }

            StickerButton {
                id: settingsButton

                anchors {
                    right: helpButton.left
                    rightMargin: Theme.space3
                    verticalCenter: parent.verticalCenter
                }
                glyph: "settings"
                glyphStroke: Theme.fineGlyphStroke
                Accessible.name: qsTr("Settings")
                tip: qsTr("Settings (Ctrl+,)")
                onClicked: settingsDialog.open()
            }

            // Help: debug info, logs and bug reports.
            StickerButton {
                id: helpButton

                anchors {
                    right: rightControls.left
                    rightMargin: Theme.space4
                    verticalCenter: parent.verticalCenter
                }
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
                        text: qsTr("What's new")
                        visible: Release.notes !== ""
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

                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    rightMargin: Theme.space4
                }
                visible: buttons.length > 0
                buttons: WindowButtons.right
            }
        }

        Item {
            id: columns

            // The columns give way on a narrow window, so the note keeps room to write in.
            readonly property int room: width - Theme.editorMinWidth
            readonly property int sidebarWidth: Math.max(0, Math.min(Settings.sidebarWidth > 0
                                                                     ? Settings.sidebarWidth :
                                                                       Theme.sidebarWidth, room
                                                                     - Settings.minNoteListWidth))
            readonly property int noteListWidth: Math.max(0, Math.min(Settings.noteListWidth > 0
                                                                      ? Settings.noteListWidth :
                                                                        Theme.noteListWidth, room
                                                                      - sidebarWidth))

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

                onNoteMade: editor.editTitle()
                onJoplinImportRequested: folder => joplinImport.start(folder)
                visible: Vault.isOpen
                anchors {
                    left: parent.left
                    top: parent.top
                    bottom: parent.bottom
                }
                width: columns.sidebarWidth
            }
            ColumnSplitter {
                visible: sidebar.visible
                anchors {
                    horizontalCenter: sidebar.right
                    top: parent.top
                    bottom: parent.bottom
                }
                columnWidth: sidebar.width
                onResized: width => Settings.sidebarWidth = width
                onReset: Settings.resetSidebarWidth()
            }
            TaskList {
                visible: Vault.isOpen && Vault.showingTasks
                anchors {
                    left: sidebar.right
                    top: parent.top
                    bottom: parent.bottom
                }
                width: columns.noteListWidth
            }
            NoteList {
                id: noteList

                onTitleEditRequested: editor.editTitle()
                visible: Vault.isOpen && !Vault.showingTasks
                anchors {
                    left: sidebar.right
                    top: parent.top
                    bottom: parent.bottom
                }
                width: columns.noteListWidth
            }
            ColumnSplitter {
                visible: Vault.isOpen
                anchors {
                    horizontalCenter: noteList.right
                    top: parent.top
                    bottom: parent.bottom
                }
                columnWidth: noteList.width
                onResized: width => Settings.noteListWidth = width
                onReset: Settings.resetNoteListWidth()
            }
            NoteEditor {
                id: editor

                router: keys
                onLeftNote: noteList.focusList()

                visible: Vault.isOpen
                anchors {
                    left: noteList.right
                    right: parent.right
                    top: parent.top
                    bottom: parent.bottom
                }
            }
        }
    }

    QuickSwitcher {
        id: switcher
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
        id: reportDialog
        sourceComponent: ReportDialog {}
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

    // ---- Keyboard ----------------------------------------------------------------
    /// Does what the keyboard asked for, by KeyRouter's action id.
    function run(id) {
        const note = Vault.current !== ""
        switch (id) {
        case "newNote":
            if (Vault.isOpen && Vault.writable && Vault.newNote() !== "")
                editor.editTitle()
            break
        case "goTo":
            if (Vault.isOpen)
                switcher.open()
            break
        case "search":
            if (Vault.isOpen)
                noteList.focusSearch()
            break
        case "save":
            if (Vault.isOpen)
                Vault.saveNow()
            break
        case "tasks":
            if (Vault.isOpen)
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
            if (Vault.isOpen && !Vault.showingTasks)
                noteList.step(id)
            break
        case "editNote":
            if (note)
                editor.write()
            break
        case "rename":
            if (note && Vault.writable)
                editor.editTitle()
            break
        case "trash":
            // Not while the tasks are shown, which hide the note it would ask about.
            if (note && Vault.writable && !Vault.showingTasks)
                noteList.askToTrash(Vault.current)
            break
        case "sourceMode":
            Settings.sourceMode = !Settings.sourceMode
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
