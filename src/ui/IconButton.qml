import Jot.Ui
import QtQuick
import QtQuick.Controls

/// A bare icon to press, as the note's trash is: red for what deletes, tinted when hovered.
AbstractButton {
    id: root

    /// A Glyph name.
    property string glyph
    /// Red, for what deletes; the text's color otherwise.
    property bool danger: true

    implicitWidth: 32
    implicitHeight: 32
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }

    background: Rectangle {
        radius: Theme.radiusSm
        readonly property color tone: root.danger ? Theme.danger : Theme.text
        color: root.pressed ? Theme.tint(tone, 0.3) : root.hovered || root.visualFocus ? Theme.tint(
                                                                                             tone, 0.15) :
                                                                                         "transparent"
    }
    contentItem: Item {
        Glyph {
            anchors.centerIn: parent
            name: root.glyph
            color: root.danger ? Theme.danger : Theme.text
            stroke: Theme.fineGlyphStroke
            opacity: root.enabled ? 1 : 0.4
        }
    }
}
