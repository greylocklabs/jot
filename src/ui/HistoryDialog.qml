pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// The past versions of the note shown, newest first, each readable beside the list, or shown as
/// what changed between it and the note now or another version, and restorable over the note.
Popup {
    id: root

    property var versions: []
    readonly property var chosen: list.currentIndex >= 0 ? versions[list.currentIndex] : null

    /// How the comparison came out, in a sentence over it.
    function changesSummary() {
        if (comparison.error !== undefined)
            return comparison.error
        if (comparison.changes === 0)
            return qsTr("No changes.")
        if (comparison.changes === 1)
            return qsTr("Changed in one place: what went is marked -, what came +.")
        return qsTr("Changed in %1 places: what went is marked -, what came +.").arg(
                    comparison.changes)
    }

    /// The last restore did not go through.
    property bool failed: false
    /// The chosen version shown as what changed, rather than as its text.
    property bool showChanges: true
    /// What the chosen version is compared with: another version's path, or empty for now.
    property string against: ""
    // Older first: the list is newest first, so the one further down it.
    readonly property int againstIndex: versions.findIndex(version => version.path === against)
    readonly property var comparison: !chosen || !showChanges ? ({}) : against === ""
                                                                ? Vault.compareVersions(chosen.path,
                                                                                        "") : againstIndex
                                                                  > list.currentIndex
                                                                  ? Vault.compareVersions(against,
                                                                                          chosen.path) :
                                                                    Vault.compareVersions(
                                                                        chosen.path, against)

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
        against = ""
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

        Row {
            id: controls

            anchors {
                top: header.bottom
                topMargin: Theme.space4
                left: list.right
                leftMargin: Theme.space4
                right: parent.right
            }
            visible: root.versions.length > 0
            spacing: Theme.space3

            ModeSwitch {
                objectName: "historyMode"
                anchors.verticalCenter: parent.verticalCenter
                options: [
                    {
                        "label": qsTr("Changes"),
                        "tip": qsTr("What differs from the version compared with")
                    },
                    {
                        "label": qsTr("Text"),
                        "tip": qsTr("The version as it was")
                    }
                ]
                current: root.showChanges ? 0 : 1
                onPicked: index => root.showChanges = index === 0
            }
            Dropdown {
                objectName: "historyAgainst"
                anchors.verticalCenter: parent.verticalCenter
                visible: root.showChanges
                minimumWidth: 0
                maximumWidth: controls.width - x
                model: [
                    {
                        "id": "",
                        "name": qsTr("Compared with the note now")
                    }
                ].concat(root.versions.filter(version => !root.chosen || version.path
                                                         !== root.chosen.path).map(version => ({
                                                             "id": version.path,
                                                             "name": qsTr("Compared with %1").arg(
                                                                         version.label)
                                                         })))
                currentValue: root.against
                onActivated: value => root.against = value
            }
        }

        ListView {
            id: changes

            objectName: "versionChanges"
            anchors {
                top: controls.bottom
                topMargin: Theme.space3
                left: list.right
                leftMargin: Theme.space4
                right: parent.right
                bottom: restore.top
                bottomMargin: Theme.space4
            }
            visible: root.versions.length > 0 && root.showChanges
            clip: true
            model: root.comparison.lines !== undefined ? root.comparison.lines : []
            ScrollBar.vertical: ScrollBar {}

            header: Text {
                width: ListView.view.width
                bottomPadding: Theme.space2
                wrapMode: Text.Wrap
                text: root.changesSummary()
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSm
            }

            delegate: Rectangle {
                required property var modelData

                width: ListView.view.width
                height: lineText.implicitHeight
                color: modelData.kind === "same" ? "transparent" : Theme.tint(modelData.kind
                                                                              === "removed"
                                                                              ? Theme.danger :
                                                                                Theme.accent, 0.16)

                Text {
                    id: lineText
                    width: parent.width
                    leftPadding: Theme.space2
                    rightPadding: Theme.space2
                    text: (parent.modelData.kind === "removed" ? "- " : parent.modelData.kind === "added"
                                                                 ? "+ " : "  ")
                          + parent.modelData.text
                    textFormat: Text.PlainText
                    wrapMode: Text.WrapAnywhere
                    color: parent.modelData.kind === "same" ? Theme.textMuted : Theme.text
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.textSm
                }
            }
        }

        ScrollView {
            id: viewer

            anchors {
                top: controls.bottom
                topMargin: Theme.space3
                left: list.right
                leftMargin: Theme.space4
                right: parent.right
                bottom: restore.top
                bottomMargin: Theme.space4
            }
            visible: root.versions.length > 0 && !root.showChanges
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
