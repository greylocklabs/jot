pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// Adds a to-do to the inbox from a line typed as you would say it, "call Soren tomorrow 3pm
/// @minutes", showing as you type the day, time, repeat and labels it understood.
Popup {
    id: root

    readonly property var task: Vault.readQuickTask(input.text)

    function add() {
        if (root.task.title === "" || !Vault.addQuickTask(input.text))
            return
        input.text = ""
        close()
    }

    x: (parent ? parent.width - width : 0) / 2
    y: Theme.titleBarHeight + Theme.space6
    width: Math.min(parent ? parent.width - 2 * Theme.space6 : 0, Theme.searchWidth)
    padding: Theme.space4
    modal: true
    focus: true
    onOpened: input.forceActiveFocus()

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

                objectName: "quickAddInput"
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - closeButton.width - parent.spacing
                placeholderText: qsTr("Call Soren tomorrow 3pm @minutes #calls")
                Accessible.name: qsTr("New to-do")
                Keys.onReturnPressed: root.add()
                Keys.onEnterPressed: root.add()
            }
            CloseButton {
                id: closeButton
                anchors.verticalCenter: parent.verticalCenter
                onClicked: root.close()
            }
        }

        // What it understood, so a misread day shows before it is added.
        Text {
            objectName: "quickAddPreview"
            width: parent.width
            leftPadding: Theme.space2
            visible: root.task.title !== ""
            text: [root.task.when, root.task.repeat !== "" ? "🔁 " + root.task.repeat : ""].concat(
                root.task.labels.map(label => "@" + label)).filter(part => part !== "").join("   ")
            || qsTr("No day: it goes in Anytime")
            elide: Text.ElideRight
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
            font.weight: Theme.weightStrong
        }

        Row {
            spacing: Theme.space2

            StickerButton {
                accent: true
                enabled: root.task.title !== "" && Vault.writable
                text: qsTr("Add to inbox")
                onClicked: root.add()
            }
            StickerButton {
                text: qsTr("Cancel")
                onClicked: root.close()
            }
        }
    }
}
