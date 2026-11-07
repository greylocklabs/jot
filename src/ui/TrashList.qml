pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// The middle column when Trash is chosen: the notes in the vault's trash, newest first.
/// Choosing one shows it beside the list, to read, bring back or delete for good.
Rectangle {
    id: root

    /// Where the note chosen is in the trash, or empty.
    property string chosen

    color: Theme.bg

    // A note brought back or gone from the trash is no longer there to show.
    Connections {
        target: Vault.trash
        function onModelReset() {
            if (!Vault.trash.contains(root.chosen))
                root.chosen = ""
        }
    }

    Column {
        id: header

        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: Theme.space5
        }
        spacing: Theme.space2

        Item {
            width: parent.width
            height: Math.max(heading.implicitHeight, emptyTrash.implicitHeight)

            Text {
                id: heading

                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Trash")
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textLg
                font.weight: Theme.weightBold
            }
            StickerButton {
                id: emptyTrash

                objectName: "emptyTrash"
                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                }
                visible: Vault.trash.count > 0
                enabled: Vault.writable
                destructive: true
                text: qsTr("Empty trash")
                onClicked: confirm.ask(qsTr("Empty the trash?"), qsTr(
                                           "Its %n note(s) are deleted now, with their attachments, and cannot be brought back.",
                                           "", Vault.trash.count), qsTr("Empty trash"), ()
                                       => Vault.emptyTrash())
            }
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: Vault.trash.count === 0 ? qsTr("Nothing from this vault is in the trash.") :
                                            Settings.trashDays > 0 ? qsTr(
                                                                         "Notes deleted from this vault. After %n day(s) they go to your desktop's trash.",
                                                                         "", Settings.trashDays) :
                                                                     qsTr("Notes deleted from this vault. They stay until you delete them for good.")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
        }
    }

    ListView {
        id: list

        objectName: "trashList"
        anchors {
            left: parent.left
            right: parent.right
            top: header.bottom
            bottom: parent.bottom
            topMargin: Theme.space4
        }
        clip: true
        model: Vault.trash
        spacing: Theme.space1
        ScrollBar.vertical: ScrollBar {}

        delegate: AbstractButton {
            id: row

            required property string title
            required property string folder
            required property string deleted
            required property string location

            width: ListView.view.width
            height: Theme.listRowHeight * 2
            hoverEnabled: true
            Accessible.name: title
            onClicked: root.chosen = location

            background: Rectangle {
                anchors.fill: parent
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space3
                radius: Theme.radiusSm
                color: root.chosen === row.location ? Theme.tint(Theme.accent, 0.18) : row.hovered
                                                      ? Theme.surfaceAlt : "transparent"
            }
            contentItem: Item {
                Column {
                    anchors {
                        left: parent.left
                        right: parent.right
                        leftMargin: Theme.space5
                        rightMargin: Theme.space5
                        verticalCenter: parent.verticalCenter
                    }
                    spacing: Theme.space1

                    Text {
                        width: parent.width
                        text: row.title
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textMd
                        font.weight: Theme.weightStrong
                    }
                    Text {
                        width: parent.width
                        text: row.folder !== "" ? qsTr("%1, deleted %2").arg(row.folder).arg(
                                                      row.deleted) : qsTr("Deleted %1").arg(
                                                      row.deleted)
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                }
            }
        }
    }

    ConfirmDialog {
        id: confirm
    }
}
