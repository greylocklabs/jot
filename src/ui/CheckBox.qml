import Jot.Ui
import QtQuick
import QtQuick.Controls

/// A labeled box to tick, in the theme's colors: an accent fill and a check when ticked.
AbstractButton {
    id: root

    checkable: true
    implicitHeight: 32
    implicitWidth: box.width + Theme.space3 + caption.implicitWidth
    padding: 0
    Accessible.role: Accessible.CheckBox
    Accessible.checked: checked
    Accessible.name: text

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }

    contentItem: Item {
        Rectangle {
            id: box

            anchors.verticalCenter: parent.verticalCenter
            width: Theme.textBase + Theme.space1
            height: width
            radius: Theme.radiusSm
            color: root.checked ? Theme.accent : root.hovered ? Theme.tint(Theme.accent, 0.25) :
                                                                Theme.surface
            border.color: root.checked || root.hovered ? Theme.accent : Theme.border
            border.width: 2

            Glyph {
                anchors.centerIn: parent
                width: parent.width - Theme.space2
                height: width
                visible: root.checked
                name: "check"
                color: Theme.accentText
            }
        }
        Text {
            id: caption

            anchors {
                left: box.right
                leftMargin: Theme.space3
                verticalCenter: parent.verticalCenter
            }
            text: root.text
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textMd
            font.weight: Theme.weightStrong
        }
    }
}
