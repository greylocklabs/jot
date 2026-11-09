import Jot.Ui
import QtQuick
import QtQuick.Controls

/// Asks before something that cannot be taken back, such as deleting for good.
Popup {
    id: root

    property string title
    property string body
    property string confirmText
    property var onConfirm: null
    /// Whether the confirm button looks like it destroys something.
    property bool destructive: true
    /// A second choice beside confirming, such as "Don't save"; none when empty.
    property string otherText
    property var onOther: null
    /// Runs when it closes without a choice, by Cancel, Escape or the X.
    property var onDismiss: null
    property var chosen: null

    /// Opens it; `confirm` runs if the destructive button is chosen.
    function ask(title, body, confirmText, confirm) {
        root.title = title
        root.body = body
        root.confirmText = confirmText
        root.onConfirm = confirm
        open()
    }

    anchors.centerIn: Overlay.overlay
    width: 400
    padding: Theme.space6
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onClosed: {
        const run = root.chosen ?? root.onDismiss
        root.onConfirm = null
        root.onOther = null
        root.onDismiss = null
        root.chosen = null
        root.otherText = ""
        root.destructive = true
        if (run)
            run()
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

        CloseButton {
            anchors {
                top: parent.top
                right: parent.right
                margins: Theme.space4
            }
            onClicked: root.close()
        }
    }

    contentItem: Column {
        spacing: Theme.space4

        Text {
            // Room for the X in the corner.
            width: parent.width - Theme.space7
            wrapMode: Text.Wrap
            text: root.title
            color: Theme.text
            font.family: Theme.displayFontFamily
            font.pixelSize: Theme.textLg
            font.weight: Theme.weightBold
        }
        Text {
            width: parent.width
            visible: root.body !== ""
            wrapMode: Text.Wrap
            text: root.body
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
        }
        Row {
            spacing: Theme.space2

            StickerButton {
                objectName: "confirmButton"
                text: root.confirmText
                destructive: root.destructive
                armed: true
                onClicked: {
                    root.chosen = root.onConfirm ?? (() => {})
                    root.close()
                }
            }
            StickerButton {
                objectName: "otherButton"
                visible: root.otherText !== ""
                text: root.otherText
                destructive: true
                onClicked: {
                    root.chosen = root.onOther ?? (() => {})
                    root.close()
                }
            }
            StickerButton {
                objectName: "cancelButton"
                text: qsTr("Cancel")
                onClicked: root.close()
            }
        }
    }
}
