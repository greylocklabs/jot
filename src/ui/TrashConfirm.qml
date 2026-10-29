import Jot.Ui
import QtQuick
import QtQuick.Controls

/// Asks before a note or a notebook goes to the trash, as the editor's Delete does, since a
/// vault's own trash is out of sight.
Popup {
    id: root

    /// What goes: a note's path, or a notebook's when `notebook` is set.
    property string path
    property bool notebook: false
    /// The other notes that link into the note's attachments, which then stay unless asked.
    property var sharing: []

    /// Opens it for the note at `path`.
    function askForNote(path) {
        root.path = path
        root.notebook = false
        root.sharing = Vault.notesUsingAttachments(path)
        open()
    }
    /// Opens it for the notebook at `path`.
    function askForNotebook(path) {
        root.path = path
        root.notebook = true
        root.sharing = []
        open()
    }

    anchors.centerIn: Overlay.overlay
    width: root.sharing.length > 0 ? 460 : 380
    padding: Theme.space6
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Scrim {}

    background: Rectangle {
        radius: Theme.radiusXl
        color: Theme.surface
        border.color: Theme.border

        SoftShadow {
            anchors.fill: parent
            radius: parent.radius
        }

        // In the corner, clear of what the card says.
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
            text: {
                if (!root.notebook)
                    return qsTr("Move %1 to the trash?").arg(Vault.titleOf(root.path))
                const name = root.path.split("/").pop()
                const count = Vault.notesInNotebook(root.path)
                return count === 0 ? qsTr("Move the notebook %1 to the trash?").arg(name) : qsTr(
                                         "Move the notebook %1 and its %n note(s) to the trash?", "",
                                         count).arg(name)
            }
            color: Theme.text
            font.family: Theme.displayFontFamily
            font.pixelSize: Theme.textLg
            font.weight: Theme.weightBold
        }
        Text {
            objectName: "sharedAttachments"
            visible: root.sharing.length > 0
            width: parent.width
            wrapMode: Text.Wrap
            text: {
                if (root.sharing.length === 0)
                    return ""
                const first = Vault.titleOf(root.sharing[0])
                return root.sharing.length === 1 ? qsTr(
                                                       "%1 links to its attachments, so they stay where they are unless you trash them too.").arg(
                                                       first) : qsTr(
                                                       "%1 other notes link to its attachments, %2 among them, so they stay where they are unless you trash them too.").arg(
                                                       root.sharing.length).arg(first)
            }
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: root.notebook ? qsTr(
                                      "It goes to your desktop's trash with everything in it. Where the desktop has none for this folder, it goes to the .jot/trash folder in your notes folder.") :
                                  root.sharing.length > 0 ? qsTr(
                                                                "It goes to your desktop's trash. Where the desktop has none for this folder, it goes to the .jot/trash folder in your notes folder.") :
                                                            qsTr("It goes to your desktop's trash, with its attachments. Where the desktop has none for this folder, it goes to the .jot/trash folder in your notes folder.")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
        }
        Row {
            spacing: Theme.space2

            StickerButton {
                objectName: "confirmTrash"
                text: qsTr("Move to trash")
                destructive: true
                armed: true
                onClicked: {
                    if (root.notebook)
                        Vault.trashNotebook(root.path)
                    else
                        Vault.trashNote(root.path, root.sharing.length > 0)
                    root.close()
                }
            }
            StickerButton {
                objectName: "trashAttachmentsToo"
                visible: root.sharing.length > 0
                text: qsTr("Trash attachments too")
                destructive: true
                onClicked: {
                    Vault.trashNote(root.path, false)
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
