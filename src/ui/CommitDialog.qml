pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// Commits what changed to a git remote that commits by hand: shows the files and takes a
/// message, filled in from the vault's commit message template.
Popup {
    id: root

    property var files: []

    function commit() {
        if (message.text.trim() === "" || root.files.length === 0)
            return
        Vault.sync.commit(message.text)
        close()
    }

    objectName: "commitDialog"
    anchors.centerIn: Overlay.overlay
    width: Math.min(Overlay.overlay ? Overlay.overlay.width - 2 * Theme.space6 : 520, 520)
    padding: Theme.space4
    modal: true
    focus: true
    onAboutToShow: {
        root.files = Vault.sync.changes()
        message.text = Vault.sync.suggestedMessage(root.files)
    }
    onOpened: {
        message.forceActiveFocus()
        message.selectAll()
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

        Item {
            width: parent.width
            height: closeButton.height

            Text {
                anchors {
                    left: parent.left
                    right: closeButton.left
                    rightMargin: Theme.space2
                    verticalCenter: parent.verticalCenter
                }
                leftPadding: Theme.space2
                text: qsTr("Commit")
                elide: Text.ElideRight
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textMd
                font.weight: Theme.weightBold
            }
            CloseButton {
                id: closeButton
                anchors.right: parent.right
                onClicked: root.close()
            }
        }

        Text {
            width: parent.width
            leftPadding: Theme.space2
            wrapMode: Text.Wrap
            text: root.files.length === 0 ? qsTr("Nothing has changed since the last commit.") :
                                            qsTr("%n file(s) changed:", "", root.files.length)
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
        }
        ListView {
            objectName: "commitFiles"
            visible: root.files.length > 0
            width: parent.width
            height: Math.min(contentHeight, 160)
            clip: true
            model: root.files
            ScrollBar.vertical: ScrollBar {}

            delegate: Text {
                required property string modelData

                width: ListView.view.width
                leftPadding: Theme.space4
                text: modelData
                textFormat: Text.PlainText
                elide: Text.ElideMiddle
                color: Theme.text
                font.family: Theme.monoFontFamily
                font.pixelSize: Theme.textSm
            }
        }
        Field {
            id: message

            objectName: "commitMessage"
            width: parent.width
            placeholderText: qsTr("What changed")
            Accessible.name: qsTr("Commit message")
            Keys.onReturnPressed: root.commit()
            Keys.onEnterPressed: root.commit()
        }
        Row {
            spacing: Theme.space2

            StickerButton {
                objectName: "commitNow"
                accent: true
                enabled: root.files.length > 0 && message.text.trim() !== ""
                text: Vault.sync.manualPushes ? qsTr("Commit") : qsTr("Commit and push")
                onClicked: root.commit()
            }
            StickerButton {
                text: qsTr("Cancel")
                onClicked: root.close()
            }
        }
    }
}
