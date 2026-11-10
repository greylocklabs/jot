pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// Sets the words a notebook, with the notebooks inside it, is to come to, and the day to reach
/// them by. The footer under its notes shows how it is going.
Popup {
    id: root

    /// The notebook's path in the vault.
    property string path
    /// The day to reach the target by; invalid for none.
    property date deadline: new Date(NaN)
    /// Whether the notebook had a target when the dialog opened, which can be taken away.
    property bool hasTarget
    readonly property int words: Number(input.text.replace(/[^0-9]/g, "")) || 0

    function openFor(path) {
        root.path = path
        const target = Vault.notebookTarget(path)
        root.hasTarget = target.words > 0
        input.text = target.words > 0 ? String(target.words) : ""
        root.deadline = target.deadline !== "" ? Date.fromLocaleString(Qt.locale(), target.deadline,
                                                                       "yyyy-MM-dd") : new Date(NaN)
        open()
    }
    function set() {
        if (Vault.setNotebookTarget(root.path, root.words, isNaN(root.deadline.getTime()) ? "" :
                                                                                            Qt.formatDate(
                                                                                                root.deadline,
                                                                                                "yyyy-MM-dd")))
            close()
    }

    anchors.centerIn: Overlay.overlay
    width: Math.min(Overlay.overlay ? Overlay.overlay.width - 2 * Theme.space6 : 480,
                    Theme.searchWidth)

    padding: Theme.space4
    modal: true
    focus: true
    onOpened: {
        input.forceActiveFocus()
        input.selectAll()
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

        Item {
            width: parent.width
            height: closeButton.height

            Text {
                anchors {
                    left: parent.left
                    right: closeButton.left
                    rightMargin: Theme.space2
                    verticalCenter: parent.verticalCenter
                }
                leftPadding: Theme.space2
                text: qsTr("Word target for %1").arg(root.path.split("/").pop())
                elide: Text.ElideRight
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textMd
                font.weight: Theme.weightBold
            }
            CloseButton {
                id: closeButton
                anchors.right: parent.right
                onClicked: root.close()
            }
        }

        Row {
            width: parent.width
            spacing: Theme.space2

            Field {
                id: input

                objectName: "targetWords"
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - deadlinePicker.width - noDeadline.width - 2 * parent.spacing
                placeholderText: qsTr("90000")
                inputMethodHints: Qt.ImhDigitsOnly
                Accessible.name: qsTr("Words to reach")
                Keys.onReturnPressed: root.set()
                Keys.onEnterPressed: root.set()
            }
            DatePicker {
                id: deadlinePicker

                objectName: "targetDeadline"
                anchors.verticalCenter: parent.verticalCenter
                day: root.deadline
                placeholder: qsTr("Add a deadline")
                focusPolicy: Qt.NoFocus
                onPicked: day => root.deadline = day
            }
            StickerButton {
                id: noDeadline

                anchors.verticalCenter: parent.verticalCenter
                visible: !isNaN(root.deadline.getTime())
                glyph: "close"
                focusPolicy: Qt.NoFocus
                Accessible.name: qsTr("No deadline")
                tip: Accessible.name
                onClicked: root.deadline = new Date(NaN)
            }
        }

        Text {
            width: parent.width
            leftPadding: Theme.space2
            wrapMode: Text.Wrap
            text: qsTr(
                      "Counts the words of every note in this notebook and the notebooks inside it.")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
        }

        Row {
            spacing: Theme.space2

            StickerButton {
                objectName: "targetSet"
                accent: true
                enabled: Vault.writable && root.words > 0
                text: qsTr("Set")
                onClicked: root.set()
            }
            StickerButton {
                objectName: "targetRemove"
                visible: root.hasTarget
                enabled: Vault.writable
                text: qsTr("Remove target")
                onClicked: {
                    if (Vault.setNotebookTarget(root.path, 0, ""))
                        root.close()
                }
            }
            StickerButton {
                text: qsTr("Cancel")
                onClicked: root.close()
            }
        }
    }
}
