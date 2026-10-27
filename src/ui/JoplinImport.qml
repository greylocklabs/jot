import Jot.Ui
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

/// Brings a Joplin export into the notes folder: choose the .jex, see what importing it would
/// do, then import. Importing the same export again is safe, so the summary says what is new.
Popup {
    id: root

    /// The notebook the export goes into; "" is the top of the notes folder.
    property string folder
    property url file
    /// What importing would do, then what it did, as Vault.importJoplin() gives it.
    property var summary: ({})
    property bool imported: false

    /// Asks for an export to bring into the notebook at `folder`.
    function start(folder) {
        root.folder = folder
        fileDialog.open()
    }

    function counts(s) {
        const parts = [s.created === 1 ? qsTr("1 new note") : qsTr("%1 new notes").arg(s.created)]
        if (s.updated > 0)
            parts.push(qsTr("%1 updated").arg(s.updated))
        if (s.notebooks > 0)
            parts.push(s.notebooks === 1 ? qsTr("1 new notebook") : qsTr("%1 new notebooks").arg(
                                               s.notebooks))
        if (s.attachments > 0)
            parts.push(s.attachments === 1 ? qsTr("1 attachment") : qsTr("%1 attachments").arg(
                                                 s.attachments))
        return parts.join(", ")
    }

    anchors.centerIn: Overlay.overlay
    width: 440
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
            text: !root.summary.ok ? qsTr("Could not import from Joplin") : root.imported ? qsTr("Imported from Joplin") :
                                                                                            qsTr("Import from Joplin?")
            color: Theme.text
            font.family: Theme.displayFontFamily
            font.pixelSize: Theme.textLg
            font.weight: Theme.weightBold
        }
        Text {
            objectName: "importCounts"
            width: parent.width
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            text: root.summary.ok ? root.counts(root.summary) : (root.summary.error ?? "")
            color: root.summary.ok ? Theme.text : Theme.danger
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textMd
        }
        // The details, close together: what stays as it is, by name, so nothing seems lost.
        Column {
            width: parent.width
            spacing: Theme.space1

            Text {
                visible: root.summary.ok === true && root.summary.unchanged > 0
                width: parent.width
                wrapMode: Text.Wrap
                text: root.summary.unchanged === 1 ? qsTr("1 note unchanged since the last import") :
                                                     qsTr("%1 notes unchanged since the last import").arg(
                                                         root.summary.unchanged)
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSm
            }
            Repeater {
                model: (root.summary.keptEdits ?? []).map(path => qsTr("Keeps your edits to %1").arg(
                                                                      path)).concat((
                                                                                        root.summary.notBroughtBack
                                                                                        ?? []).map(
                                                                                        path => qsTr(
                                                                                                    "Does not bring back %1, removed since").arg(
                                                                                                    path))).concat(
                           (root.summary.skipped ?? []).map(why => qsTr("Leaves out %1").arg(why)))
                delegate: Text {
                    required property string modelData
                    width: parent.width
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    text: modelData
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textSm
                }
            }
        }
        Text {
            visible: root.summary.ok === true && !root.imported
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTr(
                      "Where Joplin recorded each note being written is left out. You can import again later; only what changed in Joplin comes across.")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
        }
        Row {
            spacing: Theme.space2

            StickerButton {
                objectName: "confirmImport"
                accent: true
                visible: root.summary.ok === true && !root.imported
                enabled: Vault.writable
                text: qsTr("Import")
                onClicked: {
                    root.summary = Vault.importJoplin(root.file, root.folder, false)
                    root.imported = root.summary.ok === true
                }
            }
            StickerButton {
                text: root.imported || !root.summary.ok ? qsTr("Done") : qsTr("Cancel")
                onClicked: root.close()
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Choose a Joplin export")
        nameFilters: [qsTr("Joplin exports (*.jex)")]
        onAccepted: {
            root.file = selectedFile
            root.imported = false
            root.summary = Vault.importJoplin(selectedFile, root.folder, true)
            root.open()
        }
    }
}
