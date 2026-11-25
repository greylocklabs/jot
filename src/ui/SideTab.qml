import Jot.Ui
import QtQuick
import QtQuick.Controls

/// One of the tabs down the left of a dialog, as Settings and Keyboard shortcuts have.
AbstractButton {
    id: root

    property string label
    property bool selected: false

    // Its label's width, which sizes the list; a bare AbstractButton has none of its own on
    // older Qt.
    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    height: Theme.listRowHeight + Theme.space2
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: label
    Accessible.role: Accessible.PageTab

    background: Rectangle {
        radius: Theme.radiusMd
        color: root.selected ? Theme.tint(Theme.accent, 0.18) : root.hovered ? Theme.surfaceAlt :
                                                                               "transparent"

        // Ringed while the keyboard is on it, as the pill buttons are.
        border.width: root.visualFocus ? 2 : 0
        border.color: Theme.text
    }
    contentItem: Text {
        leftPadding: Theme.space3
        rightPadding: Theme.space6
        verticalAlignment: Text.AlignVCenter
        text: root.label
        color: root.selected ? Theme.text : Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textMd
        font.weight: root.selected ? Theme.weightBold : Theme.weightStrong
    }
}
