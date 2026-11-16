import Jot.Ui
import QtQuick
import QtQuick.Controls

/// A menu item in the theme's colors, for menus like the title bar's ? menu.
MenuItem {
    id: root

    /// The menu's padding around its entries. The highlight's corners follow
    /// the menu's rounded ones at that distance, so they never cross its border.
    readonly property real inset: menu ? menu.padding : 0

    /// Marks this entry as the current choice, with a check on the right. It
    /// is not `checked`, which a click would toggle out from under its binding.
    property bool current: false
    /// Keeps room for that check, so entries line up whichever is current.
    property bool marks: false
    /// An icon before the text, as a Glyph names it, or none.
    property string glyph
    property color glyphColor: Theme.textMuted

    implicitHeight: 30
    leftPadding: Theme.space4
    rightPadding: Theme.space4 + (root.marks ? 14 + Theme.space2 : 0)

    contentItem: Item {
        implicitWidth: (root.glyph !== "" ? icon.width + Theme.space2 : 0) + label.implicitWidth
        implicitHeight: label.implicitHeight

        Glyph {
            id: icon

            anchors.verticalCenter: parent.verticalCenter
            visible: root.glyph !== ""
            name: root.glyph
            color: root.glyphColor
            stroke: Theme.smallGlyphStroke
            implicitWidth: Theme.smallGlyphSize
            implicitHeight: Theme.smallGlyphSize
        }
        Text {
            id: label

            anchors {
                left: parent.left
                leftMargin: root.glyph !== "" ? icon.width + Theme.space2 : 0
                right: parent.right
                top: parent.top
                bottom: parent.bottom
            }
            text: root.text
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
            font.weight: root.current ? Theme.weightBold : Font.Normal
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
    }

    // A submenu's entry points to where it opens.
    arrow: Glyph {
        x: root.width - width - Theme.space3
        anchors.verticalCenter: parent.verticalCenter
        visible: root.subMenu !== null
        name: "chevron-right"
        color: Theme.textFaint
        stroke: Theme.smallGlyphStroke
        implicitWidth: Theme.smallGlyphSize
        implicitHeight: Theme.smallGlyphSize
    }

    indicator: Glyph {
        x: root.width - width - Theme.space4
        anchors.verticalCenter: parent.verticalCenter
        visible: root.current
        name: "check"
        color: Theme.accent
    }

    background: Rectangle {
        radius: Math.max(Theme.radiusSm, Theme.radiusLg - root.inset)
        color: root.highlighted ? Theme.surfaceAlt : "transparent"
    }
}
