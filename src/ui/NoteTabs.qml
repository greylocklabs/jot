pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// The notes open in tabs, over the note, while there is more than one.
Rectangle {
    id: root

    implicitHeight: Theme.listRowHeight + 2 * Theme.space2
    color: Theme.bg

    Rectangle {
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: 1
        color: Theme.hairline
    }

    ListView {
        id: list

        objectName: "tabList"
        anchors {
            fill: parent
            leftMargin: Theme.space3
            rightMargin: Theme.space3
            topMargin: Theme.space2
            bottomMargin: Theme.space2
        }
        orientation: ListView.Horizontal
        spacing: Theme.space1
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: Vault.tabs
        currentIndex: Vault.tabIndex
        highlightFollowsCurrentItem: false
        onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)

        delegate: Item {
            id: tab

            required property string modelData
            required property int index

            readonly property bool shown: index === Vault.tabIndex

            objectName: "tab"
            width: Math.min(220, Math.max(120, (list.width - (Vault.tabs.length - 1) * list.spacing)
                                          / Vault.tabs.length))
            height: list.height
            Accessible.role: Accessible.PageTab
            Accessible.name: label.text

            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusMd
                color: tab.shown ? Theme.tint(Theme.accent, 0.18) : tabHover.hovered
                                   ? Theme.surfaceAlt : "transparent"
            }
            Text {
                id: label

                anchors {
                    left: parent.left
                    leftMargin: Theme.space3
                    right: close.left
                    rightMargin: Theme.space1
                    verticalCenter: parent.verticalCenter
                }
                elide: Text.ElideRight
                textFormat: Text.PlainText
                text: Vault.titleOf(tab.modelData)
                color: tab.shown ? Theme.text : Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSm
                font.weight: tab.shown ? Theme.weightBold : Theme.weightStrong
            }
            AbstractButton {
                id: close

                objectName: "closeTab"
                anchors {
                    right: parent.right
                    rightMargin: Theme.space1
                    verticalCenter: parent.verticalCenter
                }
                implicitWidth: 20
                implicitHeight: 20
                // Shown on the tab shown, or under the pointer, as other editors do.
                opacity: tab.shown || tabHover.hovered || hovered ? 1 : 0
                focusPolicy: Qt.NoFocus
                Accessible.name: qsTr("Close %1").arg(label.text)
                onClicked: Vault.closeTab(tab.index)

                background: Rectangle {
                    radius: Theme.radiusSm
                    color: close.hovered ? Theme.surfaceAlt : "transparent"
                }
                contentItem: Glyph {
                    name: "close"
                    color: Theme.textMuted
                    stroke: Theme.fineGlyphStroke
                }
            }
            HoverHandler {
                id: tabHover
            }
            TapHandler {
                onTapped: Vault.selectTab(tab.index)
            }
            TapHandler {
                acceptedButtons: Qt.MiddleButton
                onTapped: Vault.closeTab(tab.index)
            }
        }
    }
}
