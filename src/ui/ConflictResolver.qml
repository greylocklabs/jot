pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// The note shown beside another version of it, kept when the two could not be merged, line by
/// line with what differs marked, to keep one or the other.
Popup {
    id: root

    /// The other version: `{path, from, when}`, as Vault.currentConflicts gives it.
    property var copy: null
    property var comparison: ({})
    readonly property var rows: comparison.rows !== undefined ? comparison.rows : []

    function openFor(copy) {
        root.copy = copy
        root.comparison = Vault.compareWith(copy.path)
        open()
    }

    anchors.centerIn: Overlay.overlay
    width: Math.min(1000, (Overlay.overlay ? Overlay.overlay.width : 1000) - 2 * Theme.space6)
    height: Math.min(640, (Overlay.overlay ? Overlay.overlay.height : 640) - 2 * Theme.space6)
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
    }

    contentItem: Column {
        spacing: Theme.space4

        Item {
            width: parent.width
            height: heading.implicitHeight

            Text {
                id: heading
                width: parent.width - closeButton.width - Theme.space3
                text: qsTr("Two versions of %1").arg(Vault.currentTitle)
                elide: Text.ElideRight
                color: Theme.text
                font.family: Theme.displayFontFamily
                font.pixelSize: Theme.textXl
                font.weight: Theme.weightBold
            }
            CloseButton {
                id: closeButton
                anchors.right: parent.right
                anchors.verticalCenter: heading.verticalCenter
                onClicked: root.close()
            }
        }

        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: root.comparison.error !== undefined ? root.comparison.error : root.comparison.differences
                                                        === 1 ? qsTr(
                                                                    "They differ in one place, marked below.") :
                                                                qsTr("They differ in %1 places, marked below.").arg(
                                                                    root.comparison.differences)
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
        }

        Row {
            id: labels

            width: parent.width
            spacing: Theme.space4

            Repeater {
                model: [qsTr("This note"), root.copy ? [qsTr("The other version"), root.copy.from
                                                        !== "" ? qsTr("from %1").arg(
                                                                     root.copy.from) : "",
                                                        root.copy.when].filter(part => part
                                                                                       !== "").join(
                                                           ", ") : ""]

                Text {
                    required property string modelData
                    width: (labels.width - labels.spacing) / 2
                    text: modelData
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: Theme.textFaint
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textXs
                    font.weight: Theme.weightHeavy
                    font.capitalization: Font.AllUppercase
                }
            }
        }

        ListView {
            id: lines

            objectName: "conflictLines"
            width: parent.width
            height: root.availableHeight - y - actions.height - parent.spacing
            clip: true
            model: root.rows
            ScrollBar.vertical: ScrollBar {}

            delegate: Row {
                id: line

                required property var modelData
                readonly property bool differs: modelData.change !== "same"

                width: ListView.view.width
                spacing: Theme.space4

                Repeater {
                    model: 2

                    Rectangle {
                        id: side

                        required property int index
                        // A line only the other side has leaves a gap here.
                        readonly property bool absent: line.modelData.change === (index === 0 ? "theirs" :
                                                                                                "mine")

                        width: (line.width - line.spacing) / 2
                        height: Math.max(cell.implicitHeight, Theme.textSm * 1.6)
                        color: !line.differs ? "transparent" : absent ? Theme.surfaceAlt : Theme.tint(
                                                                            index === 0
                                                                            ? Theme.danger :
                                                                              Theme.accent, 0.16)

                        Text {
                            id: cell
                            width: parent.width
                            leftPadding: Theme.space2
                            rightPadding: Theme.space2
                            text: side.absent ? "" : side.index === 0 ? line.modelData.mine :
                                                                        line.modelData.theirs
                            textFormat: Text.PlainText
                            wrapMode: Text.WrapAnywhere
                            color: line.differs ? Theme.text : Theme.textMuted
                            font.family: Theme.monoFontFamily
                            font.pixelSize: Theme.textSm
                        }
                    }
                }
            }
        }

        Flow {
            id: actions

            width: parent.width
            spacing: Theme.space2

            StickerButton {
                objectName: "keepThisVersion"
                accent: true
                enabled: Vault.writable
                text: qsTr("Keep this note")
                tip: qsTr("The other version goes to the trash")
                onClicked: {
                    if (Vault.keepThisVersion(root.copy.path))
                        root.close()
                }
            }
            StickerButton {
                objectName: "useOtherVersion"
                enabled: Vault.writable
                text: qsTr("Use the other version")
                tip: qsTr("This note is kept in its history, and the copy goes to the trash")
                onClicked: {
                    if (Vault.useOtherVersion(root.copy.path))
                        root.close()
                }
            }
            StickerButton {
                text: qsTr("Decide later")
                onClicked: root.close()
            }
        }
    }
}
