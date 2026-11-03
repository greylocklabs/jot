pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// Sets the day a to-do in a note is due, and how it repeats, from words typed as quick add
/// reads them, "friday 3pm every week", or a day picked, showing what it understood.
Popup {
    id: root

    /// The to-do's words after its box.
    property string task
    readonly property var said: Vault.readQuickTask(input.text)
    readonly property bool clearing: input.text.trim() === ""

    /// `line` is the to-do's words with its new day and repeat.
    signal chosen(string line)

    function openFor(task) {
        root.task = task
        input.text = Vault.scheduleWords(task)
        open()
    }
    function set() {
        const line = Vault.scheduledAs(root.task, input.text)
        // Cleared, a to-do of nothing but its markers has no words left.
        if (line === "" && !root.clearing)
            return
        close()
        root.chosen(line)
    }

    x: (parent ? parent.width - width : 0) / 2
    y: Theme.space6
    width: Math.min(parent ? parent.width - 2 * Theme.space6 : 0, Theme.searchWidth)
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

        Row {
            width: parent.width
            spacing: Theme.space2

            Field {
                id: input

                objectName: "scheduleInput"
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - picker.width - closeButton.width - 2 * parent.spacing
                placeholderText: qsTr("Friday 3pm every week")
                Accessible.name: qsTr("When the to-do is due")
                Keys.onReturnPressed: root.set()
                Keys.onEnterPressed: root.set()
            }
            DatePicker {
                id: picker

                anchors.verticalCenter: parent.verticalCenter
                placeholder: qsTr("Pick a day")
                focusPolicy: Qt.NoFocus
                // The day picked, keeping the repeat typed.
                onPicked: day => input.text = [Qt.formatDate(day, "yyyy-MM-dd"),
                                               root.said.repeat].filter(part => part !== "").join(
                                     " ")
            }
            CloseButton {
                id: closeButton
                anchors.verticalCenter: parent.verticalCenter
                onClicked: root.close()
            }
        }

        // What it understood, so a misread day shows before it is set.
        Text {
            objectName: "schedulePreview"
            width: parent.width
            leftPadding: Theme.space2
            text: {
                if (root.clearing)
                    return qsTr("No day and no repeat")
                const parts = [root.said.when, root.said.repeat !== "" ? "🔁 " + root.said.repeat :
                                                                         ""]

                return parts.filter(part => part !== "").join("   ") || qsTr(
                            "No day or repeat in that")
            }
            elide: Text.ElideRight
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
            font.weight: Theme.weightStrong
        }

        Row {
            spacing: Theme.space2

            StickerButton {
                objectName: "scheduleSet"
                accent: true
                enabled: Vault.writable && (root.clearing || root.said.due !== ""
                                            || root.said.repeat !== "")
                text: root.clearing ? qsTr("Clear") : qsTr("Set")
                onClicked: root.set()
            }
            StickerButton {
                text: qsTr("Cancel")
                onClicked: root.close()
            }
        }
    }
}
