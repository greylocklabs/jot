pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// Chooses the emoji shown before a notebook's name, found by typing words of its name.
Popup {
    id: root

    /// The notebook it is for.
    property string path
    property string query

    /// Asks for an emoji for the notebook at `path`.
    function choose(path) {
        root.path = path
        open()
    }

    function pick(emoji) {
        Vault.setNotebookEmoji(root.path, emoji)
        close()
    }

    function pickHighlighted() {
        const highlighted = grid.model[grid.currentIndex]
        if (highlighted)
            pick(highlighted.emoji)
    }

    anchors.centerIn: Overlay.overlay
    width: 420
    padding: Theme.space5
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: {
        input.text = ""
        root.query = ""
        grid.currentIndex = 0
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
                placeholderText: qsTr("Find an emoji")
                onTextEdited: {
                    root.query = text
                    grid.currentIndex = 0
                }
                Keys.onDownPressed: grid.moveCurrentIndexDown()
                Keys.onUpPressed: grid.moveCurrentIndexUp()
                Keys.onRightPressed: grid.moveCurrentIndexRight()
                Keys.onLeftPressed: grid.moveCurrentIndexLeft()
                Keys.onReturnPressed: root.pickHighlighted()
                Keys.onEnterPressed: root.pickHighlighted()
            }
            CloseButton {
                id: closeButton
                anchors.verticalCenter: parent.verticalCenter
                onClicked: root.close()
            }
        }

        GridView {
            id: grid

            readonly property int cell: Math.floor(width / 9)

            width: parent.width
            height: cell * 6
            cellWidth: cell
            cellHeight: cell
            clip: true
            model: Emoji.matching(root.query)
            currentIndex: 0
            // The arrow keys move through it from the search field, which keeps the typing.
            keyNavigationEnabled: false

            delegate: AbstractButton {
                id: cell

                required property var modelData
                required property int index
                readonly property string emoji: modelData.emoji

                width: grid.cell
                height: grid.cell
                hoverEnabled: true
                Accessible.name: modelData.name
                onClicked: root.pick(emoji)

                background: Rectangle {
                    radius: Theme.radiusSm
                    color: cell.GridView.isCurrentItem ? Theme.tint(Theme.accent, 0.18) :
                                                         cell.hovered ? Theme.surfaceAlt :
                                                                        Theme.surface
                }
                contentItem: Text {
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: cell.emoji
                    font.pixelSize: Theme.textLg * 1.4
                }
                Tip {
                    visible: cell.hovered
                    text: cell.modelData.name
                    delay: Theme.durSlow * 2
                }
            }
        }

        Text {
            visible: grid.count === 0
            leftPadding: Theme.space2
            text: qsTr("No emoji matches.")
            color: Theme.textFaint
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textMd
        }

        Row {
            spacing: Theme.space2

            StickerButton {
                visible: (Vault.notebookEmoji[root.path] ?? "") !== ""
                text: qsTr("No emoji")
                onClicked: root.pick("")
            }
            StickerButton {
                text: qsTr("Cancel")
                onClicked: root.close()
            }
        }
    }
}
