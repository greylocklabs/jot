pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

/// Jot's preferences. Every change applies and saves at once, so there is
/// nothing to confirm; reset puts everything back.
Popup {
    id: root

    anchors.centerIn: Overlay.overlay
    // One size for every tab, so the dialog does not jump between them; a tab longer than that
    // scrolls.
    width: Math.min(Overlay.overlay ? Overlay.overlay.width - 2 * Theme.space6 : 720, 720)
    height: Math.min(Overlay.overlay ? Overlay.overlay.height - 2 * Theme.space6 : 620, 620)
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    /// The user wants to edit the theme's colors, which needs the window in view.
    signal editColorsRequested
    signal joplinImportRequested

    /// Which group of settings shows: "general", "sync", "appearance", "keyboard" or "developer".
    property string tab: "general"
    /// The tab list's width: its longest label's.
    readonly property real tabsWidth: {
        let widest = 0
        for (let i = 0; i < tabRepeater.count; ++i)
            widest = Math.max(widest, tabRepeater.itemAt(i) ? tabRepeater.itemAt(i).implicitWidth :
                                                              0)
        return widest
    }

    Overlay.modal: Scrim {}

    enter: Transition {
        NumberAnimation {
            property: "scale"
            from: 0.94
            to: 1
            duration: Theme.durMed
            easing.type: Theme.easingBounce
            easing.overshoot: Theme.bounce
        }
        NumberAnimation {
            property: "opacity"
            from: 0
            to: 1
            duration: Theme.durFast
        }
    }

    background: Rectangle {
        radius: Theme.radiusXl
        color: Theme.surface
        border.color: Theme.border

        SoftShadow {
            anchors.fill: parent
            radius: parent.radius
        }
    }

    contentItem: Item {
        Item {
            id: header

            x: Theme.space6
            y: Theme.space6
            width: parent.width - 2 * Theme.space6
            height: title.implicitHeight

            Text {
                id: title
                text: qsTr("Settings")
                color: Theme.text
                font.family: Theme.displayFontFamily
                font.pixelSize: Theme.textXl
                font.weight: Theme.weightBold
            }

            CloseButton {
                anchors {
                    right: parent.right
                    verticalCenter: title.verticalCenter
                }
                onClicked: root.close()
            }
        }

        // The tabs down the left, each label starting under the title, its highlight reaching
        // out past it.
        Column {
            id: tabs

            anchors {
                left: parent.left
                leftMargin: Theme.space6 - Theme.space3
                top: header.bottom
                topMargin: Theme.space6
            }
            width: root.tabsWidth
            spacing: Theme.space2

            Repeater {
                id: tabRepeater

                model: [
                    {
                        "id": "general",
                        "label": qsTr("General")
                    },
                    {
                        "id": "sync",
                        "label": qsTr("Sync")
                    },
                    {
                        "id": "appearance",
                        "label": qsTr("Theme")
                    },
                    {
                        "id": "keyboard",
                        "label": qsTr("Keyboard")
                    },
                    {
                        "id": "developer",
                        "label": qsTr("Developer")
                    }
                ]

                AbstractButton {
                    id: tabButton

                    required property var modelData
                    readonly property bool selected: root.tab === modelData.id

                    objectName: "settingsTab"
                    // Its label's width, which sizes the list; a bare AbstractButton has none
                    // of its own on older Qt.
                    implicitWidth: implicitContentWidth + leftPadding + rightPadding
                    width: tabs.width
                    height: Theme.listRowHeight + Theme.space2
                    hoverEnabled: true
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: modelData.label
                    Accessible.role: Accessible.PageTab
                    onClicked: root.tab = modelData.id

                    background: Rectangle {
                        radius: Theme.radiusMd
                        color: tabButton.selected ? Theme.tint(Theme.accent, 0.18) :
                                                    tabButton.hovered ? Theme.surfaceAlt :
                                                                        "transparent"
                        // Ringed while the keyboard is on it, as the pill buttons are.
                        border.width: tabButton.visualFocus ? 2 : 0
                        border.color: Theme.text
                    }
                    contentItem: Text {
                        leftPadding: Theme.space3
                        rightPadding: Theme.space6
                        verticalAlignment: Text.AlignVCenter
                        text: tabButton.modelData.label
                        color: tabButton.selected ? Theme.text : Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textMd
                        font.weight: tabButton.selected ? Theme.weightBold : Theme.weightStrong
                    }
                }
            }
        }

        ScrollView {
            id: pane

            anchors {
                left: tabs.right
                leftMargin: Theme.space6
                right: parent.right
                top: header.bottom
                topMargin: Theme.space6
                bottom: parent.bottom
            }
            contentWidth: availableWidth
            clip: true

            Column {
                width: pane.availableWidth - Theme.space6
                bottomPadding: Theme.space6
                spacing: Theme.space5

                Section {
                    title: qsTr("Notes folder")
                    visible: root.tab === "general"

                    Text {
                        width: parent.width
                        text: Vault.isOpen ? Vault.root : qsTr("No folder chosen yet")
                        textFormat: Text.PlainText
                        elide: Text.ElideMiddle
                        color: Vault.isOpen ? Theme.text : Theme.textMuted
                        font.family: Theme.monoFontFamily
                        font.pixelSize: Theme.textSm
                    }
                    Text {
                        visible: Vault.isOpen && !Vault.writable
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: qsTr("Jot can read this folder but not change it.")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                    Flow {
                        width: parent.width
                        spacing: Theme.space2

                        StickerButton {
                            text: qsTr("Change folder...")
                            onClicked: folderDialog.open()
                        }
                        StickerButton {
                            visible: Vault.isOpen
                            text: qsTr("Show in Files")
                            onClicked: Support.openFolder("vault")
                        }
                        StickerButton {
                            visible: Vault.isOpen
                            enabled: Vault.writable
                            text: qsTr("Import from Joplin...")
                            onClicked: root.joplinImportRequested()
                        }
                    }
                    Text {
                        visible: Vault.error !== ""
                        width: parent.width
                        text: Vault.error
                        wrapMode: Text.Wrap
                        color: Theme.danger
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                }

                Section {
                    title: qsTr("Saving")
                    visible: root.tab === "general"

                    Toggle {
                        objectName: "autosave"
                        width: parent.width
                        text: qsTr("Save as you type")
                        checked: Settings.autosave
                        onToggled: Settings.autosave = checked
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: Settings.autosave ? qsTr(
                                                      "Notes are written to their files moments after you stop typing.") :
                                                  qsTr("Notes are written only when you save with Ctrl+S. Jot asks before leaving a note with changes not yet saved, and sync leaves them out.")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                }

                Section {
                    title: qsTr("Writing")
                    visible: root.tab === "general"

                    Row {
                        spacing: Theme.space3

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Daily word goal")
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textMd
                        }
                        Dropdown {
                            objectName: "dailyWordGoal"
                            anchors.verticalCenter: parent.verticalCenter
                            minimumWidth: 160
                            model: [
                                {
                                    "id": 0,
                                    "name": qsTr("No goal")
                                },
                                {
                                    "id": 250,
                                    "name": qsTr("%1 words").arg(Number(250).toLocaleString(
                                                                     Qt.locale(), "f", 0))
                                },
                                {
                                    "id": 500,
                                    "name": qsTr("%1 words").arg(Number(500).toLocaleString(
                                                                     Qt.locale(), "f", 0))
                                },
                                {
                                    "id": 750,
                                    "name": qsTr("%1 words").arg(Number(750).toLocaleString(
                                                                     Qt.locale(), "f", 0))
                                },
                                {
                                    "id": 1000,
                                    "name": qsTr("%1 words").arg(Number(1000).toLocaleString(
                                                                     Qt.locale(), "f", 0))
                                },
                                {
                                    "id": 1500,
                                    "name": qsTr("%1 words").arg(Number(1500).toLocaleString(
                                                                     Qt.locale(), "f", 0))
                                },
                                {
                                    "id": 2000,
                                    "name": qsTr("%1 words").arg(Number(2000).toLocaleString(
                                                                     Qt.locale(), "f", 0))
                                },
                                {
                                    "id": 3000,
                                    "name": qsTr("%1 words").arg(Number(3000).toLocaleString(
                                                                     Qt.locale(), "f", 0))
                                }
                            ]
                            currentValue: Settings.dailyWordGoal
                            Accessible.name: qsTr("Daily word goal")
                            onActivated: value => Settings.dailyWordGoal = value
                        }
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: qsTr(
                                  "Under each note, its words, those written today and the days in a row you met the goal, on any computer syncing these notes. A note's own target is a target key in its front matter, such as target: 2000.")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                    Toggle {
                        objectName: "spellcheck"
                        width: parent.width
                        text: qsTr("Check spelling")
                        checked: Settings.spellcheck
                        onToggled: Settings.spellcheck = checked
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: qsTr(
                                  "In the desktop's language, or a note's own as lang: en_GB in its front matter, or the vault's as language: in .jot/vault.yaml. Right-click a word to correct it.")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                    Toggle {
                        objectName: "focusMode"
                        width: parent.width
                        text: qsTr("Fade all but the paragraph being written")
                        checked: Settings.focusMode
                        onToggled: Settings.focusMode = checked
                    }
                    Toggle {
                        objectName: "typewriter"
                        width: parent.width
                        text: qsTr("Keep the line being written in the middle")
                        checked: Settings.typewriter
                        onToggled: Settings.typewriter = checked
                    }
                }

                Section {
                    title: qsTr("Sync")
                    visible: root.tab === "sync"

                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: qsTr(
                                  "Keep these notes the same as a copy elsewhere, in another folder, such as a NAS, a USB drive or a network share, or in an S3 bucket, and so on other computers syncing with it. Jot syncs when it opens, shortly after you write, and every few minutes. Where both copies changed, it merges them, or keeps both.")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                    Text {
                        visible: !Vault.sync.configured
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: qsTr(
                                  "Notes in a folder that Syncthing, Nextcloud or Dropbox already syncs need nothing set up here: when those apps keep two versions of a note, Jot shows both to compare.")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                    Text {
                        visible: Vault.sync.configured
                        width: parent.width
                        text: Vault.sync.remote
                        textFormat: Text.PlainText
                        elide: Text.ElideMiddle
                        color: Theme.text
                        font.family: Theme.monoFontFamily
                        font.pixelSize: Theme.textSm
                    }
                    Text {
                        objectName: "syncSummary"
                        visible: Vault.sync.configured
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: Vault.sync.summary
                        color: Vault.sync.problem !== "" ? Theme.danger : Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                    Flow {
                        width: parent.width
                        spacing: Theme.space2

                        StickerButton {
                            visible: Vault.sync.configured
                            enabled: !Vault.sync.syncing
                            accent: true
                            text: qsTr("Sync now")
                            onClicked: Vault.sync.syncNow()
                        }
                        StickerButton {
                            enabled: Vault.isOpen && Vault.writable
                            text: Vault.sync.configured ? qsTr("Sync with a folder instead...") :
                                                          qsTr("Sync with a folder...")
                            onClicked: syncFolderDialog.open()
                        }
                        StickerButton {
                            objectName: "syncWithDropbox"
                            visible: Vault.sync.dropboxAvailable
                            enabled: Vault.isOpen && Vault.writable
                            text: qsTr("Sync with Dropbox...")
                            onClicked: {
                                dropboxForm.visible = !dropboxForm.visible
                                bucketForm.visible = false
                                driveForm.visible = false
                                gitForm.visible = false
                            }
                        }
                        StickerButton {
                            objectName: "syncWithDrive"
                            visible: Vault.sync.driveAvailable
                            enabled: Vault.isOpen && Vault.writable && !Vault.sync.checking
                            text: qsTr("Sync with Google Drive...")
                            onClicked: {
                                driveForm.visible = !driveForm.visible
                                bucketForm.visible = false
                                dropboxForm.visible = false
                                gitForm.visible = false
                            }
                        }
                        StickerButton {
                            objectName: "syncWithGit"
                            visible: Vault.sync.gitAvailable
                            enabled: Vault.isOpen && Vault.writable
                            text: qsTr("Sync with git...")
                            onClicked: {
                                gitForm.visible = !gitForm.visible
                                bucketForm.visible = false
                                dropboxForm.visible = false
                                driveForm.visible = false
                            }
                        }
                        StickerButton {
                            objectName: "syncWithBucket"
                            enabled: Vault.isOpen && Vault.writable
                            text: qsTr("Sync with S3...")
                            onClicked: {
                                bucketForm.visible = !bucketForm.visible
                                driveForm.visible = false
                                dropboxForm.visible = false
                                gitForm.visible = false
                            }
                        }
                        StickerButton {
                            visible: Vault.sync.configured
                            text: qsTr("Stop syncing")
                            onClicked: Vault.sync.stop()
                        }
                    }
                    // A git repository anywhere, reached as git reaches it from a terminal.
                    Column {
                        id: gitForm

                        objectName: "gitForm"
                        visible: false
                        width: parent.width
                        spacing: Theme.space2

                        Text {
                            width: parent.width
                            wrapMode: Text.Wrap
                            text: qsTr(
                                      "A git repository on GitHub, GitLab, Codeberg, your own server or a NAS. Jot reaches it as git does from a terminal, with your SSH keys or saved logins, and keeps large files in Git LFS. Each sync is a commit.")
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textSm
                        }
                        Field {
                            id: gitUrl
                            objectName: "gitUrl"
                            width: parent.width
                            placeholderText: qsTr("Address, such as git@github.com:you/notes.git")
                        }
                        Field {
                            id: gitBranch
                            objectName: "gitBranch"
                            width: parent.width
                            placeholderText: qsTr("Branch (main)")
                        }
                        StickerButton {
                            objectName: "connectGit"
                            accent: true
                            enabled: !Vault.sync.checking && gitUrl.text.trim() !== ""
                            text: Vault.sync.checking ? qsTr("Checking...") : qsTr("Connect")
                            onClicked: {
                                bucketForm.connecting = true
                                Vault.sync.connectGit(gitUrl.text, gitBranch.text)
                            }
                        }
                    }

                    // Jot's own folder in the user's Dropbox, let in by a code pasted back.
                    Column {
                        id: dropboxForm

                        objectName: "dropboxForm"
                        visible: false
                        width: parent.width
                        spacing: Theme.space2

                        Text {
                            width: parent.width
                            wrapMode: Text.Wrap
                            text: qsTr(
                                      "Jot keeps the notes in a folder of its own in your Dropbox, Apps/Jot, and sees nothing else there. Open Dropbox to let it in, then paste the code Dropbox shows.")
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textSm
                        }
                        StickerButton {
                            text: qsTr("Open Dropbox...")
                            onClicked: Qt.openUrlExternally(Vault.sync.startDropbox())
                        }
                        Field {
                            id: dropboxCode
                            objectName: "dropboxCode"
                            width: parent.width
                            placeholderText: qsTr("The code Dropbox shows")
                        }
                        Field {
                            id: dropboxFolder
                            objectName: "dropboxFolder"
                            width: parent.width
                            placeholderText: qsTr("Folder in Apps/Jot (optional)")
                        }
                        StickerButton {
                            objectName: "connectDropbox"
                            accent: true
                            enabled: !Vault.sync.checking && dropboxCode.text.trim() !== ""
                            text: Vault.sync.checking ? qsTr("Checking...") : qsTr("Connect")
                            onClicked: {
                                bucketForm.connecting = true
                                Vault.sync.connectDropbox(dropboxCode.text, dropboxFolder.text)
                            }
                        }
                    }

                    // A folder for the vault in Jot's folder in Google Drive.
                    Column {
                        id: driveForm
                        objectName: "driveForm"
                        visible: false
                        width: parent.width
                        spacing: Theme.space2

                        Text {
                            width: parent.width
                            wrapMode: Text.Wrap
                            text: qsTr(
                                      "Jot keeps the notes in a folder of its own in your Drive, Jot, and sees nothing else there. Give this vault the same folder name on each computer.")
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textSm
                        }
                        Field {
                            id: driveFolder
                            objectName: "driveFolder"
                            width: parent.width
                            text: Vault.name
                            placeholderText: qsTr("Folder in Jot")
                        }
                        StickerButton {
                            objectName: "connectDrive"
                            accent: true
                            enabled: !Vault.sync.checking
                            text: Vault.sync.checking ? qsTr("Waiting for Google...") : qsTr(
                                                            "Sign in to Google...")
                            onClicked: {
                                bucketForm.connecting = true
                                Vault.sync.connectDrive(driveFolder.text)
                            }
                        }
                    }

                    // An S3-compatible bucket: AWS, Cloudflare R2, Backblaze B2, MinIO, Wasabi.
                    Column {
                        id: bucketForm

                        objectName: "bucketForm"

                        /// Connect was pressed, here or for Dropbox, and the check it started has not
                        /// finished.
                        property bool connecting: false

                        visible: false
                        width: parent.width
                        spacing: Theme.space2

                        Text {
                            width: parent.width
                            wrapMode: Text.Wrap
                            text: qsTr(
                                      "An S3-compatible bucket, such as on AWS, Cloudflare R2, Backblaze B2, Wasabi or MinIO. The store must refuse writes over objects it was not told of, which Jot checks first. The key goes in your desktop's keyring.")
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textSm
                        }
                        Field {
                            id: bucketEndpoint
                            objectName: "bucketEndpoint"
                            width: parent.width
                            placeholderText: qsTr(
                                                 "Address, such as https://s3.us-east-1.amazonaws.com")
                        }
                        Row {
                            width: parent.width
                            spacing: Theme.space2

                            Field {
                                id: bucketRegion
                                objectName: "bucketRegion"
                                width: (parent.width - parent.spacing) / 3
                                placeholderText: qsTr("Region, or auto")
                            }
                            Field {
                                id: bucketName
                                objectName: "bucketName"
                                width: (parent.width - parent.spacing) * 2 / 3
                                placeholderText: qsTr("Bucket")
                            }
                        }
                        Field {
                            id: bucketFolder
                            objectName: "bucketFolder"
                            width: parent.width
                            placeholderText: qsTr("Folder in the bucket (optional)")
                        }
                        Field {
                            id: bucketKey
                            objectName: "bucketKey"
                            width: parent.width
                            placeholderText: qsTr("Access key ID")
                        }
                        Field {
                            id: bucketSecret
                            objectName: "bucketSecret"
                            width: parent.width
                            placeholderText: qsTr("Secret access key")
                            echoMode: TextInput.Password
                        }
                        StickerButton {
                            objectName: "connectBucket"
                            accent: true
                            enabled: !Vault.sync.checking && bucketEndpoint.text !== ""
                                     && bucketRegion.text !== "" && bucketName.text !== ""
                                     && bucketKey.text !== "" && bucketSecret.text !== ""
                            text: Vault.sync.checking ? qsTr("Checking...") : qsTr("Connect")
                            onClicked: {
                                bucketForm.connecting = true
                                Vault.sync.syncWithBucket(bucketEndpoint.text, bucketRegion.text,
                                                          bucketName.text, bucketFolder.text,
                                                          bucketKey.text, bucketSecret.text)
                            }
                        }
                        Connections {
                            target: Vault.sync
                            function onChanged() {
                                // Connected: the form, and the secret typed into it, go. Only once the
                                // check that Connect started is over, not on every change to sync.
                                if (!bucketForm.connecting || Vault.sync.checking)
                                    return
                                bucketForm.connecting = false
                                if (Vault.sync.problem === "") {
                                    bucketSecret.text = ""
                                    dropboxCode.text = ""
                                    bucketForm.visible = false
                                    dropboxForm.visible = false
                                    driveForm.visible = false
                                    gitForm.visible = false
                                }
                            }
                        }
                    }

                    Text {
                        visible: !Vault.sync.configured && Vault.sync.problem !== ""
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: Vault.sync.problem
                        color: Theme.danger
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                }

                Section {
                    title: qsTr("Updates")
                    visible: root.tab === "general" && !Release.devel

                    Toggle {
                        width: parent.width
                        text: qsTr("Say when a new release is out")
                        checked: Settings.checkForUpdates
                        onToggled: Settings.checkForUpdates = checked
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: qsTr(
                                  "Once a day, Jot asks GitHub for its latest release. The app shows it in the ? menu, and the jot command after it runs.")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                }

                Section {
                    title: qsTr("Trash")
                    visible: root.tab === "general"

                    Row {
                        spacing: Theme.space3

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Keep deleted notes")
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textMd
                        }
                        Dropdown {
                            objectName: "trashDays"
                            anchors.verticalCenter: parent.verticalCenter
                            minimumWidth: 260
                            model: [
                                {
                                    "id": 0,
                                    "name": qsTr("Until I delete them")
                                },
                                {
                                    "id": 30,
                                    "name": qsTr("30 days, then in the desktop's trash")
                                }
                            ]
                            currentValue: Settings.trashDays
                            Accessible.name: qsTr("Keep deleted notes")
                            onActivated: value => Settings.trashDays = value
                        }
                    }
                    Toggle {
                        objectName: "searchTrash"
                        width: parent.width
                        text: qsTr("Include the trash in search results")
                        checked: Settings.searchTrash
                        onToggled: Settings.searchTrash = checked
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: qsTr(
                                  "Deleted notes go to Jot's own trash, in the .jot/trash folder of your notes, which is never synced.")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                }

                Section {
                    title: qsTr("Reminders")
                    visible: root.tab === "general"

                    Toggle {
                        width: parent.width
                        text: qsTr("Remind me when a to-do comes due")
                        checked: Settings.remindersOn
                        onToggled: Settings.remindersOn = checked
                    }
                    Row {
                        spacing: Theme.space3
                        enabled: Settings.remindersOn

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("For those due on a day without a time, at")
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textMd
                        }
                        Field {
                            id: reminderTime

                            anchors.verticalCenter: parent.verticalCenter
                            width: 72
                            text: Settings.reminderTime
                            inputMethodHints: Qt.ImhTime
                            Accessible.name: qsTr("Reminder time for to-dos without a time")
                            // A time Settings cannot read goes back to the one it has.
                            onEditingFinished: {
                                Settings.reminderTime = text
                                text = Settings.reminderTime
                            }
                        }
                    }
                    Toggle {
                        width: parent.width
                        text: qsTr("Keep running in the tray when the window closes")
                        checked: Settings.keepRunning
                        onToggled: Settings.keepRunning = checked
                    }
                    WhyDisabled {
                        width: parent.width
                        visible: StartAtLogin.available
                        target: startAtLogin
                        reason: qsTr(
                                    "Jot starts at login only to keep running in the tray, so turn that on first.")

                        Toggle {
                            id: startAtLogin

                            width: parent.width
                            enabled: Settings.keepRunning
                            text: qsTr("Start Jot when I log in")
                            checked: StartAtLogin.enabled
                            onToggled: StartAtLogin.enabled = checked
                        }
                    }
                    Text {
                        width: parent.width
                        visible: StartAtLogin.error !== ""
                        wrapMode: Text.Wrap
                        text: StartAtLogin.error
                        color: Theme.danger
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: qsTr(
                                  "Reminders come while Jot is running, with its window open or in the tray, and those that came due while it was closed are told when it starts.")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                }

                Section {
                    title: qsTr("Theme")
                    visible: root.tab === "appearance"

                    Dropdown {
                        model: Themes.available
                        currentValue: Themes.current
                        Accessible.name: qsTr("Theme")
                        onActivated: value => Themes.use(value)
                    }

                    Flow {
                        width: parent.width
                        spacing: Theme.space2

                        StickerButton {
                            text: Themes.editable ? qsTr("Edit colors") : qsTr("Customize colors")
                            onClicked: {
                                if (Themes.customize())
                                    root.editColorsRequested()
                            }
                        }
                        StickerButton {
                            text: qsTr("Import...")
                            onClicked: importDialog.open()
                        }
                        StickerButton {
                            text: qsTr("Export...")
                            onClicked: exportDialog.open()
                        }
                        // Deleting takes a second click, since the file is gone for good.
                        DangerButton {
                            visible: Themes.editable
                            label: qsTr("Delete theme")
                            armedLabel: qsTr("Click again to delete")
                            onConfirmed: Themes.removeCurrent()
                        }
                    }

                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: qsTr(
                                  "Themes are shared with Callie, so a theme file works in either app.")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }

                    Text {
                        visible: Themes.error !== ""
                        width: parent.width
                        text: Themes.error
                        wrapMode: Text.Wrap
                        color: Theme.danger
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                }

                Section {
                    title: qsTr("Note text")
                    visible: root.tab === "appearance"

                    Row {
                        spacing: Theme.space2

                        Repeater {
                            model: [
                                {
                                    "id": "",
                                    "label": qsTr("Theme font")
                                },
                                {
                                    "id": "serif",
                                    "label": qsTr("Serif")
                                },
                                {
                                    "id": "mono",
                                    "label": qsTr("Mono")
                                }
                            ]

                            PillButton {
                                id: fontChoice
                                required property var modelData
                                label: fontChoice.modelData.label
                                selected: Settings.noteFont === fontChoice.modelData.id
                                onClicked: Settings.noteFont = fontChoice.modelData.id
                            }
                        }
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: qsTr(
                                  "The font notes are written in. Serif suits long writing; mono lines up columns of text.")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                }

                Section {
                    title: qsTr("Quick capture")
                    visible: root.tab === "keyboard"

                    Toggle {
                        objectName: "quickCaptureKey"
                        width: parent.width
                        visible: QuickCapture.available
                        text: QuickCapture.trigger !== "" ? qsTr(
                                                                "Add a to-do from anywhere with %1").arg(
                                                                QuickCapture.trigger) : qsTr(
                                                                "Add a to-do from anywhere with a key of its own")
                        checked: Settings.quickCaptureKey
                        onToggled: Settings.quickCaptureKey = checked
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: QuickCapture.available ? qsTr(
                                                           "Your desktop asks to allow it the first time, and lets you choose another key in its own shortcut settings. Or bind any key to jot-gui --quick-add there.") :
                                                       qsTr("Bind a key to jot-gui --quick-add in your desktop's shortcut settings to add a to-do from anywhere.")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                }

                Section {
                    title: qsTr("Vi mode")
                    visible: root.tab === "keyboard"

                    Toggle {
                        width: parent.width
                        text: qsTr("Move around and edit with single keys")
                        checked: Settings.viMode
                        onToggled: Settings.viMode = checked
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: qsTr(
                                  "Notes open in normal mode: h, j, k and l move, i or a starts typing, Esc stops, v selects. Out of a note, j and k go through the list. The leader key starts a command; ? lists them all.")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                    Row {
                        enabled: Settings.viMode
                        opacity: enabled ? 1 : Theme.fadedOpacity
                        spacing: Theme.space2

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Leader")
                            color: Theme.textFaint
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textSm
                            font.weight: Theme.weightHeavy
                        }
                        Repeater {
                            model: [
                                {
                                    "key": ",",
                                    "label": ","
                                },
                                {
                                    "key": ":",
                                    "label": ":"
                                },
                                {
                                    "key": " ",
                                    "label": qsTr("Space")
                                }
                            ]

                            PillButton {
                                id: leaderChoice
                                required property var modelData
                                label: leaderChoice.modelData.label
                                selected: Settings.leaderKey === leaderChoice.modelData.key
                                onClicked: Settings.leaderKey = leaderChoice.modelData.key
                            }
                        }
                    }
                    Row {
                        enabled: Settings.viMode
                        opacity: enabled ? 1 : Theme.fadedOpacity
                        spacing: Theme.space2

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Wait for the next key")
                            color: Theme.textFaint
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textSm
                            font.weight: Theme.weightHeavy
                        }
                        StickerButton {
                            anchors.verticalCenter: parent.verticalCenter
                            glyph: "minimize"
                            Accessible.name: qsTr("Wait half a second less")
                            enabled: Settings.leaderTimeout > 0
                            onClicked: Settings.leaderTimeout = Math.max(0, Settings.leaderTimeout
                                                                         - 500)
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            // As wide as its longest wording, so the buttons beside it stay put.
                            width: Math.max(implicitWidth, longest.advanceWidth)
                            horizontalAlignment: Text.AlignHCenter

                            TextMetrics {
                                id: longest
                                text: qsTr("until Esc")
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.textMd
                                font.weight: Theme.weightBold
                            }

                            text: Settings.leaderTimeout === 0 ? qsTr("until Esc") : qsTr(
                                                                     "%1 s").arg(Number(
                                                                                     Settings.leaderTimeout
                                                                                     / 1000).toLocaleString(
                                                                                     Qt.locale(),
                                                                                     "f", 1))
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textMd
                            font.weight: Theme.weightBold
                        }
                        StickerButton {
                            anchors.verticalCenter: parent.verticalCenter
                            glyph: "plus"
                            Accessible.name: qsTr("Wait half a second more")
                            onClicked: Settings.leaderTimeout += 500
                        }
                    }
                }

                Section {
                    title: qsTr("Developer")
                    visible: root.tab === "developer"

                    Toggle {
                        objectName: "developerMode"
                        width: parent.width
                        text: qsTr("Developer mode")
                        checked: Settings.developerMode
                        onToggled: Settings.developerMode = checked
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: qsTr(
                                  "Shows each note's file path in the vault under its title, to copy.")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                    Toggle {
                        objectName: "verboseLogging"
                        width: parent.width
                        text: qsTr("Log debug details")
                        checked: Settings.verboseLogging
                        onToggled: Settings.verboseLogging = checked
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: qsTr(
                                  "Writes Jot's debug lines to the log. They can include note titles and paths, so turn it off again before sharing logs widely.")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                }

                Section {
                    title: qsTr("Folders")
                    visible: root.tab === "developer"

                    Flow {
                        width: parent.width
                        spacing: Theme.space2

                        StickerButton {
                            text: qsTr("Settings")
                            onClicked: Support.openFolder("config")
                        }
                        StickerButton {
                            text: qsTr("Search index")
                            onClicked: Support.openFolder("cache")
                        }
                        StickerButton {
                            text: qsTr("Logs")
                            onClicked: Support.openLogs()
                        }
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: qsTr(
                                  "Your notes are only ever the files in your notes folder. The search index is rebuilt from them whenever it is missing.")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                }

                DangerButton {
                    visible: root.tab === "general"
                    label: qsTr("Reset to defaults")
                    armedLabel: qsTr("Click again to reset")
                    // Settings forget the theme too, so the default one comes back.
                    onConfirmed: {
                        Settings.reset()
                        Themes.useDefault()
                    }
                }
            }
        }
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Choose a folder for your notes")
        currentFolder: Vault.rootUrl
        onAccepted: Vault.openFolder(selectedFolder)
    }

    FolderDialog {
        id: syncFolderDialog
        title: qsTr("Choose a folder to sync with")
        onAccepted: Vault.sync.syncWithFolder(selectedFolder)
    }

    FileDialog {
        id: importDialog
        title: qsTr("Import a theme")
        nameFilters: [qsTr("Themes (*.toml *.yaml *.yml)")]
        onAccepted: Themes.importFrom(selectedFile)
    }

    FileDialog {
        id: exportDialog
        title: qsTr("Export this theme")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "toml"
        nameFilters: [qsTr("Themes (*.toml)")]
        onAccepted: Themes.exportTo(selectedFile)
    }

    /// A small heading over a group of related settings.
    component Section: Column {
        id: section

        property string title
        default property alias content: rows.data

        width: parent ? parent.width : 0
        spacing: Theme.space3

        Text {
            text: section.title.toUpperCase()
            color: Theme.textFaint
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textXs
            font.weight: Theme.weightHeavy
            font.letterSpacing: 0.6
        }
        Column {
            id: rows
            width: parent.width
            spacing: Theme.space3
        }
    }
}
