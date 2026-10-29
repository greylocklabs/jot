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

    /// When the version at `index` is from, to the minute, or to the second where a version
    /// beside it is from the same minute.
    function label(index) {
        const at = (i, format) => {
            const when = versions[i].written
            const today = new Date()
            const yesterday = new Date(today.getFullYear(), today.getMonth(), today.getDate() - 1)
            let day = when.toLocaleDateString(Qt.locale(), Locale.ShortFormat)
            if (when.toDateString() === today.toDateString())
                day = qsTr("Today")
            else if (when.toDateString() === yesterday.toDateString())
                day = qsTr("Yesterday")
            return qsTr("%1, %2").arg(day).arg(Qt.formatTime(when, format))
        }
        const minutes = Qt.locale().timeFormat(Locale.ShortFormat)
        const shown = at(index, minutes)
        const before = index > 0 && at(index - 1, minutes) === shown
        const after = index + 1 < versions.length && at(index + 1, minutes) === shown
        const crowded = before || after
        return crowded ? at(index, minutes.replace("mm", "mm:ss")) : shown
    }

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
            Keys.onReturnPressed: restore.clicked()
            Keys.onEnterPressed: restore.clicked()

            delegate: SwitcherRow {
                required property int index
                required property var modelData

                width: ListView.view.width
                current: ListView.isCurrentItem
                text: root.label(index)
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
                if (enabled && Vault.restoreVersion(root.chosen.path))
                    root.close()
            }
        }
    }
}
