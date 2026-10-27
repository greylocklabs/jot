pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// Finds a note by typing a few letters of its title, an alias or its path, as Ctrl+P
/// does in many editors. Enter opens the one highlighted.
Popup {
    id: root

    function choose(row) {
        const path = Vault.switcher.pathAt(row)
        if (path !== "") {
            Vault.notes.notebook = ""
            Vault.notes.query = ""
            Vault.current = path
        }
        close()
    }

    x: (parent ? parent.width - width : 0) / 2
    y: Theme.titleBarHeight + Theme.space6
    width: Math.min(parent ? parent.width - 2 * Theme.space6 : 0, Theme.searchWidth)
    padding: Theme.space3
    modal: true
    focus: true
    onOpened: {
        input.text = ""
        Vault.switcher.query = ""
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
                placeholderText: qsTr("Go to a note")
                onTextEdited: {
                    Vault.switcher.query = text
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
            model: Vault.switcher
            currentIndex: 0

            delegate: SwitcherRow {
                required property int index
                required property string title
                required property string notebook
                required property string alias

                width: ListView.view.width
                current: ListView.isCurrentItem
                text: alias !== "" ? qsTr("%1 (%2)").arg(title).arg(alias) : title
                detail: notebook
                onClicked: root.choose(index)
            }
        }

        Text {
            visible: results.count === 0
            leftPadding: Theme.space4
            bottomPadding: Theme.space2
            text: qsTr("No note matches.")
            color: Theme.textFaint
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textMd
        }
    }
}
