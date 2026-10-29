pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// The past versions of the note shown, newest first, each readable beside the list and
/// restorable over the note.
Popup {
    id: root

    property var versions: []
    readonly property var chosen: list.currentIndex >= 0 ? versions[list.currentIndex] : null

    /// The last restore did not go through.
    property bool failed: false

    anchors.centerIn: Overlay.overlay
    width: Math.min(820, (Overlay.overlay ? Overlay.overlay.width : 900) - 2 * Theme.space6)
    height: Math.min(560, (Overlay.overlay ? Overlay.overlay.height : 600) - 2 * Theme.space6)
    padding: Theme.space6
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Scrim {}

    onAboutToShow: {
        versions = Vault.historyOf(Vault.current)
        failed = false
        list.currentIndex = versions.length > 0 ? 0 : -1
        list.forceActiveFocus()
    }

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

            width: parent.width
            height: heading.height

            Column {
                id: heading
                spacing: Theme.space1

                Text {
                    text: qsTr("Past versions")
                    color: Theme.text
                    font.family: Theme.displayFontFamily
                    font.pixelSize: Theme.textXl
                    font.weight: Theme.weightBold
                }
                Text {
                    text: Vault.currentTitle
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textSm
                    font.weight: Theme.weightBold
                }
            }
            CloseButton {
                anchors.right: parent.right
                onClicked: root.close()
            }
        }

        Text {
            anchors {
                top: header.bottom
                topMargin: Theme.space4
                left: parent.left
                right: parent.right
            }
            visible: root.versions.length === 0
            text: qsTr("No past versions yet. Jot keeps one when you start editing a note, "
                       + "then one every ten minutes while you write.")
            wrapMode: Text.Wrap
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textMd
        }

        ListView {
            id: list
            objectName: "versions"

            anchors {
                top: header.bottom
                topMargin: Theme.space4
                left: parent.left
                bottom: parent.bottom
            }
            width: 220
            visible: root.versions.length > 0
            clip: true
            model: root.versions
            keyNavigationEnabled: true
            onCurrentIndexChanged: root.failed = false
            Keys.onReturnPressed: restore.clicked()
            Keys.onEnterPressed: restore.clicked()

            delegate: SwitcherRow {
                required property int index
                required property var modelData

                width: ListView.view.width
                current: ListView.isCurrentItem
                text: modelData.label
                Accessible.name: qsTr("Version from %1").arg(text)
                onClicked: list.currentIndex = index
            }
        }

        ScrollView {
            id: viewer

            anchors {
                top: header.bottom
                topMargin: Theme.space4
                left: list.right
                leftMargin: Theme.space4
                right: parent.right
                bottom: restore.top
                bottomMargin: Theme.space4
            }
            visible: root.versions.length > 0
            clip: true

            TextArea {
                objectName: "versionText"
                text: root.chosen ? Vault.versionText(root.chosen.path) : ""
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                color: Theme.text
                selectionColor: Theme.tint(Theme.accent, 0.35)
                selectedTextColor: Theme.text
                font.family: Theme.monoFontFamily
                font.pixelSize: Theme.textMd
                Accessible.name: qsTr("The version's text")
                background: Rectangle {
                    radius: Theme.radiusMd
                    color: Theme.surfaceAlt
                }
            }
        }

        StickerButton {
            id: restore
            objectName: "restore"

            anchors {
                right: parent.right
                bottom: parent.bottom
            }
            visible: root.versions.length > 0
            accent: true
            enabled: root.chosen !== null && Vault.writable
            text: qsTr("Restore this version")
            tip: qsTr("What the note holds now is kept as a version too")
            onClicked: {
                if (!enabled)
                    return
                root.failed = !Vault.restoreVersion(root.chosen.path)
                if (!root.failed)
                    root.close()
            }
        }

        Text {
            objectName: "restoreProblem"

            anchors {
                left: viewer.left
                right: restore.left
                rightMargin: Theme.space4
                verticalCenter: restore.verticalCenter
            }
            visible: root.failed
            text: qsTr("Could not restore this version, so the note is as it was.")
            wrapMode: Text.Wrap
            color: Theme.danger
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
        }
    }
}
