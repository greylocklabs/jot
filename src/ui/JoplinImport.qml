import Jot.Ui
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

/// Brings Joplin's notes into the notes folder: from the Joplin app on this computer when there
/// is one, or from a .jex export, see what importing would do, then import. Importing again is
/// safe, so the summary says what is new. Both run in the background, and the summary shows when
/// they finish, even if the dialog was closed meanwhile.
Popup {
    id: root

    /// The notebook the export goes into; "" is the top of the notes folder.
    property string folder
    property url file
    /// What importing would do, then what it did, as Vault.importJoplin() gives it.
    property var summary: ({})
    property bool imported: false
    /// Looking at what importing would do, so there is no summary yet.
    property bool looking: false
    /// Importing, after Import was pressed.
    property bool importing: false
    readonly property bool busy: looking || importing

    /// Brings Joplin's notes into the notebook at `folder`, asking for an export when the Joplin
    /// app is not on this computer.
    function start(folder) {
        // One at a time: asking again shows the one under way.
        if (root.busy) {
            root.open()
            return
        }
        root.folder = folder
        const profile = Vault.joplinProfile()
        if (profile.toString() === "")
            fileDialog.open()
        else
            preview(profile)
    }

    function preview(file) {
        root.file = file
        root.imported = false
        root.summary = {}
        root.looking = true
        root.open()
        Vault.startJoplinImport(file, root.folder, true)
    }

    function title() {
        if (root.imported)
            return qsTr("Imported from Joplin")
        if (root.looking || root.summary.ok === true)
            return qsTr("Import from Joplin?")
        return qsTr("Could not import from Joplin")
    }

    function importLabel() {
        if (!root.importing)
            return qsTr("Import")
        if (Vault.joplinImportTotal === 0)
            return qsTr("Importing...")
        return qsTr("Importing %1 of %2...").arg(Vault.joplinImportDone).arg(
                    Vault.joplinImportTotal)
    }

    function counts(s) {
        const parts = [s.created === 1 ? qsTr("1 new note") : qsTr("%1 new notes").arg(s.created)]
        if (s.updated > 0)
            parts.push(qsTr("%1 updated").arg(s.updated))
        if (s.notebooks > 0)
            parts.push(s.notebooks === 1 ? qsTr("1 new notebook") : qsTr("%1 new notebooks").arg(
                                               s.notebooks))
        if (s.versions > 0)
            parts.push(s.versions === 1 ? qsTr("1 past version") : qsTr("%1 past versions").arg(
                                              s.versions))
        if (s.attachments > 0)
            parts.push(s.attachments === 1 ? qsTr("1 attachment") : qsTr("%1 attachments").arg(
                                                 s.attachments))
        return parts.join(", ")
    }

    Connections {
        target: Vault
        enabled: root.busy
        function onJoplinImportFinished(summary) {
            root.summary = summary
            root.imported = root.importing && summary.ok === true
            root.looking = false
            root.importing = false
            root.open()
        }
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
            text: root.title()
            color: Theme.text
            font.family: Theme.displayFontFamily
            font.pixelSize: Theme.textLg
            font.weight: Theme.weightBold
        }
        Text {
            objectName: "importSource"
            width: parent.width
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            visible: text !== ""
            text: root.summary.source ?? ""
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
            font.weight: Theme.weightBold
        }
        Text {
            objectName: "importCounts"
            width: parent.width
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            text: root.looking ? qsTr("Looking at what to import...") : root.summary.ok
                                 ? root.counts(root.summary) : (root.summary.error ?? "")
            color: root.summary.ok || root.looking ? Theme.text : Theme.danger
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
                enabled: Vault.writable && !root.busy
                text: root.importLabel()
                onClicked: {
                    root.importing = true
                    // One that cannot start, as when the note being edited cannot be saved, finishes
                    // at once with the reason.
                    Vault.startJoplinImport(root.file, root.folder, false)
                }
            }
            StickerButton {
                // Closing leaves an import running; the summary shows when it finishes.
                text: root.busy ? qsTr("Close") : root.imported || !root.summary.ok ? qsTr("Done") :
                                                                                      qsTr("Cancel")
                onClicked: root.close()
            }
            StickerButton {
                objectName: "chooseExport"
                visible: !root.imported && !root.busy
                text: qsTr("Use an export...")
                tip: qsTr("Import a .jex file exported from Joplin instead")
                onClicked: {
                    root.close()
                    fileDialog.open()
                }
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Choose a Joplin export")
        nameFilters: [qsTr("Joplin exports (*.jex)")]
        onAccepted: root.preview(selectedFile)
    }
}
