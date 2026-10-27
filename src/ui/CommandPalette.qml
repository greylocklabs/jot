pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// Every command the window has, found by typing a few letters of it, as Ctrl+Shift+P does in
/// many editors. Enter runs the one highlighted.
Popup {
    id: root

    property KeyRouter router
    /// Ids of the commands that cannot run now, such as those needing a note when none is open.
    property var unavailable: []
    property string query

    /// The user chose the command with this KeyRouter action id.
    signal chosen(string id)

    function choose(row) {
        const command = results.model[row]
        close()
        if (command)
            root.chosen(command.id)
    }

    x: (parent ? parent.width - width : 0) / 2
    y: Theme.titleBarHeight + Theme.space6
    width: Math.min(parent ? parent.width - 2 * Theme.space6 : 0, Theme.searchWidth)
    padding: Theme.space3
    modal: true
    focus: true
    onOpened: {
        input.text = ""
        root.query = ""
        results.currentIndex = 0
        input.forceActiveFocus()
    }

    Overlay.modal: Scrim {}

    background: Rectangle {
        radius: Theme.radiusXl
        color: Theme.surface
        border.color: Theme.border

        SoftShadow {
            anchors.fill: parent
            radius: parent.radius
        }
    }

    contentItem: Column {
        spacing: Theme.space3

        Row {
            width: parent.width
            spacing: Theme.space2

            Field {
                id: input

                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - closeButton.width - parent.spacing
                placeholderText: qsTr("Run a command")
                onTextEdited: {
                    root.query = text
                    results.currentIndex = 0
                }
                Keys.onDownPressed: results.incrementCurrentIndex()
                Keys.onUpPressed: results.decrementCurrentIndex()
                Keys.onReturnPressed: root.choose(results.currentIndex)
                Keys.onEnterPressed: root.choose(results.currentIndex)
            }
            CloseButton {
                id: closeButton
                anchors.verticalCenter: parent.verticalCenter
                onClicked: root.close()
            }
        }

        ListView {
            id: results

            width: parent.width
            height: Math.min(contentHeight, Theme.listRowHeight * 12)
            clip: true
            model: root.router ? root.router.commands(root.query, root.unavailable) : []
            currentIndex: 0

            delegate: SwitcherRow {
                required property int index
                required property var modelData

                width: ListView.view.width
                current: ListView.isCurrentItem
                text: modelData.label
                detail: modelData.keys.length > 0 ? modelData.keys[0] : ""
                onClicked: root.choose(index)
            }
        }

        Text {
            visible: results.count === 0
            leftPadding: Theme.space4
            bottomPadding: Theme.space2
            text: qsTr("No command matches.")
            color: Theme.textFaint
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textMd
        }
    }
}
