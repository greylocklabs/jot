pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// The middle column: a search box, the order, and the notes of the notebook or tag
/// chosen in the sidebar. Choosing a note shows it.
Rectangle {
    id: root

    /// Rename was chosen for the note now shown, whose title should take the typing.
    signal titleEditRequested

    /// The notebook at `path` as the list names it, after its emoji if it has one.
    function notebookLabel(path) {
        const emoji = Vault.notebookEmoji[path] ?? ""
        return emoji !== "" ? emoji + " " + path : path
    }

    /// Opens the menu of what can be done with the note at `path`, at `x`, `y` in `item`.
    /// The point is given rather than read from the cursor, which Wayland does not share.
    function showMenu(path, item, x, y) {
        noteMenu.path = path
        noteMenu.folder = Vault.notebookOf(path)
        noteMenu.notebooks = [""].concat(Vault.notebookPaths())
        noteMenu.task = Vault.isTask(path)
        noteMenu.popup(item, x, y)
    }

    /// Puts the cursor in the search box.
    function focusSearch() {
        search.forceActiveFocus()
        search.selectAll()
    }

    /// Puts the keyboard in the list of notes.
    function focusList() {
        list.forceActiveFocus()
    }

    /// Shows the next, previous, first or last note, by KeyRouter's action id.
    function step(id) {
        if (list.count === 0)
            return
        list.forceActiveFocus()
        if (id === "firstNote" || list.currentIndex < 0)
            list.currentIndex = 0
        else if (id === "lastNote")
            list.currentIndex = list.count - 1
        else if (id === "nextNote")
            list.incrementCurrentIndex()
        else
            list.decrementCurrentIndex()
    }

    /// Asks before the note at `path` goes to the trash.
    function askToTrash(path) {
        trashConfirm.askForNote(path)
    }

    color: Theme.bg

    Column {
        id: header

        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: Theme.space4
        }
        spacing: Theme.space3

        Field {
            id: search

            width: parent.width
            placeholderText: Vault.notes.tag !== "" ? qsTr("Search #%1").arg(Vault.notes.tag) :
                                                      Vault.notes.notebook !== "" ? qsTr(
                                                                                        "Search %1").arg(
                                                                                        Vault.notes.notebook) :
                                                                                    qsTr("Search notes")
            text: Vault.notes.query
            onTextEdited: Vault.notes.query = text
            Keys.onDownPressed: list.forceActiveFocus()
            // Return keeps what was found and goes to it, as Enter after a search in vi.
            Keys.onReturnPressed: list.forceActiveFocus()
            Keys.onEnterPressed: list.forceActiveFocus()
            Keys.onEscapePressed: {
                text = ""
                Vault.notes.query = ""
                // In vi mode, Escape also gives the keys back to the list.
                if (Settings.viMode)
                    list.forceActiveFocus()
            }
        }

        Item {
            width: parent.width
            height: order.height

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: Vault.notes.count === 1 ? qsTr("1 note") : qsTr("%1 notes").arg(
                                                    Vault.notes.count)
                color: Theme.textFaint
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSm
                font.weight: Theme.weightStrong
            }
            Dropdown {
                id: order

                anchors.right: parent.right
                visible: Vault.notes.query === ""
                minimumWidth: 140
                model: [
                    {
                        "id": "updated",
                        "name": qsTr("Recently changed")
                    },
                    {
                        "id": "title",
                        "name": qsTr("By title")
                    }
                ]
                currentValue: Vault.notes.order
                onActivated: value => Vault.notes.order = value
            }
        }
    }

    ListView {
        id: list

        Keys.onPressed: event => {
            // The Menu key, or Shift+F10, opens the selected note's menu.
            const menu = event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers
                                                                                    & Qt.ShiftModifier))
            if (menu && currentItem) {
                root.showMenu(Vault.notes.pathAt(currentIndex), currentItem, Theme.space5,
                              currentItem.height / 2)
                event.accepted = true
            } else if (event.key === Qt.Key_Delete && currentItem && Vault.writable) {
                // Delete asks first, as the menu's Move to trash does.
                trashConfirm.askForNote(Vault.notes.pathAt(currentIndex))
                event.accepted = true
            }
        }

        anchors {
            left: parent.left
            right: parent.right
            top: header.bottom
            bottom: parent.bottom
            topMargin: Theme.space3
        }
        clip: true
        model: Vault.notes
        currentIndex: Vault.notes.rowOf(Vault.current)
        keyNavigationEnabled: true
        onCurrentIndexChanged: {
            if (activeFocus && currentIndex >= 0)
                Vault.current = Vault.notes.pathAt(currentIndex)
        }
        ScrollBar.vertical: ScrollBar {}

        delegate: AbstractButton {
            id: note

            required property int index
            required property string path
            required property string title
            required property string notebook
            required property date modified
            required property string snippet
            readonly property bool selected: Vault.current === path

            width: ListView.view.width
            height: card.implicitHeight + Theme.space2
            hoverEnabled: true
            Accessible.name: title

            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: eventPoint => root.showMenu(note.path, note, eventPoint.position.x,
                                                      eventPoint.position.y)
            }

            // A note drags onto a notebook in the sidebar to move there.
            // The row itself floats under the pointer while it drags: a picture of it is
            // taken as the drag starts, held where it was grabbed.
            DragHandler {
                id: noteDrag
                target: null
                enabled: Vault.writable
                onActiveChanged: {
                    if (!active) {
                        note.Drag.active = false
                        return
                    }
                    const grabbed = centroid.pressPosition
                    note.grabToImage(result => {
                        if (!noteDrag.active)
                            return
                        note.Drag.imageSource = result.url
                        note.Drag.hotSpot = grabbed
                        note.Drag.active = true
                    })
                }
            }
            Drag.dragType: Drag.Automatic
            Drag.supportedActions: Qt.MoveAction
            Drag.mimeData: ({
                                "application/x-jot-note": note.path
                            })
            onClicked: {
                Vault.current = path
                list.currentIndex = index
            }

            background: Rectangle {
                anchors.fill: parent
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space3
                radius: Theme.radiusMd
                color: note.selected ? Theme.tint(Theme.accent, 0.18) : note.hovered
                                       ? Theme.surfaceAlt : Theme.bg
            }

            contentItem: Column {
                id: card

                leftPadding: Theme.space5
                rightPadding: Theme.space5
                topPadding: Theme.space3
                bottomPadding: Theme.space3
                spacing: Theme.space1

                Text {
                    width: note.width - card.leftPadding - card.rightPadding
                    text: note.title
                    elide: Text.ElideRight
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textBase
                    font.weight: note.selected ? Theme.weightBold : Theme.weightStrong
                }
                Text {
                    width: note.width - card.leftPadding - card.rightPadding
                    visible: note.snippet !== ""
                    text: note.snippet.replace(/<mark>/g, "<b>").replace(/<\/mark>/g, "</b>")
                    textFormat: Text.StyledText
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textSm
                }
                Text {
                    width: note.width - card.leftPadding - card.rightPadding
                    text: (note.notebook !== "" && note.notebook !== Vault.notes.notebook
                           ? root.notebookLabel(note.notebook) + ", " : "")
                          + note.modified.toLocaleDateString(Qt.locale(), Locale.ShortFormat)
                    elide: Text.ElideRight
                    color: Theme.textFaint
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textXs
                }
            }
        }

        Text {
            anchors.centerIn: parent
            width: parent.width - 2 * Theme.space6
            visible: list.count === 0
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: Vault.notes.query !== "" ? qsTr("Nothing matches.") : qsTr("No notes here yet.")
            color: Theme.textFaint
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textMd
        }
    }

    Rectangle {
        anchors {
            top: parent.top
            bottom: parent.bottom
            right: parent.right
        }
        width: 1
        color: Theme.hairline
    }

    MenuCard {
        id: noteMenu

        property string path
        property var notebooks: []
        property bool task: false
        property string folder

        MenuEntry {
            text: qsTr("Open")
            onTriggered: Vault.current = noteMenu.path
        }
        MenuEntry {
            text: qsTr("Rename")
            enabled: Vault.writable
            onTriggered: {
                Vault.current = noteMenu.path
                root.titleEditRequested()
            }
        }
        MenuCard {
            id: moveMenu

            title: qsTr("Move to")
            enabled: Vault.writable

            Instantiator {
                model: noteMenu.notebooks

                MenuEntry {
                    required property string modelData
                    text: modelData === "" ? qsTr("Top of the vault") : root.notebookLabel(
                                                 modelData)
                    enabled: modelData !== noteMenu.folder
                    onTriggered: Vault.moveNoteTo(noteMenu.path, modelData)
                }
                onObjectAdded: (index, object) => moveMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => moveMenu.removeItem(object)
            }
        }
        MenuEntry {
            text: qsTr("Copy link")
            onTriggered: Support.copyText(Vault.linkTo(noteMenu.path))
        }
        MenuEntry {
            text: qsTr("Show in Files")
            onTriggered: Vault.showInFiles(noteMenu.path)
        }
        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: Theme.border
            }
        }
        MenuEntry {
            text: noteMenu.task ? qsTr("Not a task") : qsTr("Make a task")
            enabled: Vault.writable
            onTriggered: Vault.setTask(noteMenu.path, !noteMenu.task)
        }
        MenuEntry {
            text: qsTr("Move to trash...")
            enabled: Vault.writable
            onTriggered: trashConfirm.askForNote(noteMenu.path)
        }
    }

    TrashConfirm {
        id: trashConfirm
    }
}
