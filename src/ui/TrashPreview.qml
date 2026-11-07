import Jot.Ui
import QtQuick
import QtQuick.Controls

/// A note in the trash, to read before bringing it back; it cannot be changed there.
Rectangle {
    id: root

    /// Where the note is in the trash, or empty for none chosen.
    property string location

    color: Theme.bg

    Text {
        anchors.centerIn: parent
        visible: root.location === ""
        text: qsTr("Choose a note to read it or bring it back.")
        color: Theme.textFaint
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textMd
    }

    StickerButton {
        id: restore

        objectName: "restoreTrashed"
        visible: root.location !== ""
        enabled: Vault.writable
        anchors {
            top: parent.top
            right: parent.right
            margins: Theme.space5
        }
        accent: true
        text: qsTr("Restore")
        onClicked: Vault.restoreTrashed(root.location)
    }

    ScrollView {
        visible: root.location !== ""
        anchors {
            left: parent.left
            right: parent.right
            top: restore.bottom
            bottom: parent.bottom
            margins: Theme.space5
        }
        clip: true

        TextArea {
            objectName: "trashedText"
            text: root.location !== "" ? Vault.trashedText(root.location) : ""
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.Wrap
            color: Theme.text
            selectionColor: Theme.tint(Theme.accent, 0.35)
            selectedTextColor: Theme.text
            font.family: Theme.monoFontFamily
            font.pixelSize: Theme.textMd
            Accessible.name: qsTr("The note's text")
            background: Rectangle {
                radius: Theme.radiusMd
                color: Theme.surfaceAlt
            }
        }
    }
}
