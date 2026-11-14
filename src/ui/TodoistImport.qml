import Jot.Ui
import QtQuick
import QtQuick.Controls

/// Brings a Todoist account's tasks in: each project as a note in Task Lists, and the Inbox's
/// tasks into the inbox note. It looks at what importing would do first, asking for the API token
/// when none is kept or Todoist refused it, then imports. Importing again is safe, so the summary
/// says what is new. Both run in the background, and the summary shows when they finish, even if
/// the dialog was closed meanwhile.
Popup {
    id: root

    /// What importing would do, then what it did, as Vault.startTodoistImport() gives it.
    property var summary: ({})
    property bool imported: false
    /// Looking at what importing would do, so there is no summary yet.
    property bool looking: false
    /// Importing, after Import was pressed.
    property bool importing: false
    readonly property bool busy: looking || importing
    /// The token pasted here, until the keyring keeps it; empty uses the one kept.
    property string token
    /// Why Forget token could not remove it, until the next look.
    property string forgetError

    function start() {
        // One at a time: asking again shows the one under way.
        if (root.busy) {
            root.open()
            return
        }
        root.token = ""
        root.preview()
    }

    function preview() {
        root.forgetError = ""
        root.imported = false
        root.summary = {}
        root.looking = true
        root.open()
        Vault.startTodoistImport(root.token, true)
    }

    function title() {
        if (root.imported)
            return qsTr("Imported from Todoist")
        if (root.looking || root.summary.ok === true || root.summary.needsToken === true)
            return qsTr("Import from Todoist?")
        return qsTr("Could not import from Todoist")
    }

    function counts(s) {
        const parts = [s.created === 1 ? qsTr("1 new task list") : qsTr("%1 new task lists").arg(
                                             s.created)]
        if (s.updated > 0)
            parts.push(qsTr("%1 updated").arg(s.updated))
        parts.push(s.tasks === 1 ? qsTr("1 task") : qsTr("%1 tasks").arg(s.tasks))
        if (s.inboxTasks > 0)
            parts.push(s.inboxTasks === 1 ? qsTr("1 for %1").arg(s.inboxPath) : qsTr("%1 for %2").arg(
                                                s.inboxTasks).arg(s.inboxPath))
        return parts.join(", ")
    }

    Connections {
        target: Vault
        enabled: root.busy
        function onTodoistImportFinished(summary) {
            root.summary = summary
            root.imported = root.importing && summary.ok === true
            root.looking = false
            root.importing = false
            // Kept in the keyring once Todoist took it.
            if (summary.ok === true)
                root.token = ""
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
            objectName: "todoistCounts"
            width: parent.width
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            text: root.looking ? qsTr("Looking at your Todoist tasks...") : root.summary.ok
                                 ? root.counts(root.summary) : (root.summary.error ?? "")
            color: root.summary.ok || root.looking || root.summary.needsToken ? Theme.text :
                                                                                Theme.danger
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textMd
        }
        // The token, asked for only when there is none or Todoist refused the one kept.
        Column {
            width: parent.width
            spacing: Theme.space2
            visible: root.summary.needsToken === true && !root.busy

            Text {
                width: parent.width
                wrapMode: Text.Wrap
                text: qsTr(
                          "Find it in Todoist under Settings, Integrations, Developer. Jot keeps it in your keyring.")
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSm
            }
            Field {
                id: tokenField

                objectName: "todoistToken"
                width: parent.width
                echoMode: TextInput.Password
                placeholderText: qsTr("API token")
                onAccepted: if (text.trim() !== "")
                                lookButton.clicked()
            }
        }
        // The details, close together: what stays as it is, by name, so nothing seems lost.
        Column {
            width: parent.width
            spacing: Theme.space1

            Text {
                visible: root.summary.ok === true && root.summary.unchanged > 0
                width: parent.width
                wrapMode: Text.Wrap
                text: root.summary.unchanged === 1 ? qsTr(
                                                         "1 task list unchanged since the last import") :
                                                     qsTr("%1 task lists unchanged since the last import").arg(
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
            objectName: "todoistForgetError"
            visible: root.forgetError !== ""
            width: parent.width
            wrapMode: Text.Wrap
            text: root.forgetError
            color: Theme.danger
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
        }
        Text {
            visible: root.summary.ok === true && !root.imported
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTr(
                      "Completed tasks are left out. You can import again later; a task list you edited in Jot is left as it is.")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
        }
        Row {
            spacing: Theme.space2

            StickerButton {
                id: lookButton

                objectName: "lookAtTodoist"
                accent: true
                visible: root.summary.needsToken === true && !root.busy
                enabled: tokenField.text.trim() !== ""
                text: qsTr("Continue")
                onClicked: {
                    root.token = tokenField.text.trim()
                    tokenField.clear()
                    root.preview()
                }
            }
            StickerButton {
                objectName: "confirmTodoistImport"
                accent: true
                visible: root.summary.ok === true && !root.imported
                enabled: Vault.writable && !root.busy
                text: root.importing ? qsTr("Importing...") : qsTr("Import")
                onClicked: {
                    root.importing = true
                    // One that cannot start, as when the note being edited cannot be saved,
                    // finishes at once with the reason.
                    Vault.startTodoistImport(root.token, false)
                }
            }
            StickerButton {
                // Closing leaves an import running; the summary shows when it finishes.
                text: root.busy ? qsTr("Close") : root.imported || (!root.summary.ok &&
                                                                    !root.summary.needsToken) ? qsTr(
                                                                                                    "Done") : qsTr(
                                                                                                    "Cancel")
                onClicked: root.close()
            }
            StickerButton {
                objectName: "forgetTodoistToken"
                visible: root.summary.ok === true && !root.busy && root.token === ""
                text: qsTr("Forget token")
                tip: qsTr("Remove the Todoist token from your keyring")
                onClicked: {
                    // Said when the keyring would not let it go, rather than claimed gone.
                    if (!Vault.forgetTodoistToken()) {
                        root.forgetError = qsTr("Could not remove the token from your keyring.")
                        return
                    }
                    root.forgetError = ""
                    root.summary = {
                        "needsToken": true,
                        "error": qsTr("Paste your Todoist API token to import.")
                    }
                    root.imported = false
                }
            }
        }
    }
}
