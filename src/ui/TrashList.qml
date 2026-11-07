pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// The middle column when Trash is chosen: the notes in the vault's trash, newest first.
/// Choosing one shows it beside the list, to read, bring back or permanently delete.
Rectangle {
    id: root

    /// Where the note chosen is in the trash, or empty.
    readonly property string chosen: Vault.chosenTrashed

    color: Theme.bg

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
            height: Math.max(heading.implicitHeight, emptyTrash.height)

            Text {
                id: heading

                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Trash")
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textLg
                font.weight: Theme.weightBold
            }
            WhyDisabled {
                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                }
                visible: Vault.trash.total > 0
                target: emptyTrash
                reason: qsTr("This vault is from a newer Jot, so this one only reads it.")

                StickerButton {
                    id: emptyTrash

                    objectName: "emptyTrash"
                    enabled: Vault.writable
                    destructive: true
                    text: qsTr("Empty trash")
                    onClicked: confirm.ask(qsTr("Empty the trash?"), qsTr(
                                               "Its %n note(s) are deleted now, with their attachments, and cannot be brought back.",
                                               "", Vault.trash.total), qsTr("Empty trash"), ()
                                           => Vault.emptyTrash())
                }
            }
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: Vault.trash.total === 0 ? qsTr("Nothing from this vault is in the trash.") :
                                            Settings.trashDays > 0 ? qsTr(
                                                                         "Notes deleted from this vault. After %n day(s) they go to your desktop's trash.",
                                                                         "", Settings.trashDays) :
                                                                     qsTr("Notes deleted from this vault. They stay until you permanently delete them.")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
        }
        Field {
            id: search

            objectName: "trashSearch"
            width: parent.width
            visible: Vault.trash.total > 0
            placeholderText: qsTr("Search the trash")
            text: Vault.trash.query
            onTextEdited: Vault.trash.query = text
            Keys.onEscapePressed: {
                text = ""
                Vault.trash.query = ""
            }
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
            onClicked: Vault.chosenTrashed = location

            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: eventPoint => {
                    rowMenu.location = row.location
                    rowMenu.noteTitle = row.title
                    rowMenu.popup(row, eventPoint.position.x, eventPoint.position.y)
                }
            }

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

    Text {
        anchors.centerIn: list
        visible: Vault.trash.total > 0 && list.count === 0
        text: qsTr("Nothing in the trash matches.")
        color: Theme.textFaint
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textMd
    }

    MenuCard {
        id: rowMenu

        property string location
        property string noteTitle

        MenuEntry {
            text: qsTr("Restore")
            enabled: Vault.writable
            onTriggered: Vault.restoreTrashed(rowMenu.location)
        }
        MenuEntry {
            text: qsTr("Permanently delete...")
            enabled: Vault.writable
            onTriggered: {
                const location = rowMenu.location
                confirm.ask(qsTr("Permanently delete %1?").arg(rowMenu.noteTitle), qsTr(
                                "It is deleted now, with its attachments, and cannot be brought back."),
                            qsTr("Permanently delete"), () => Vault.deleteTrashed(location))
            }
        }
    }

    ConfirmDialog {
        id: confirm
    }
}
