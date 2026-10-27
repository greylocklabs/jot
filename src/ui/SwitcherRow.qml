import Jot.Ui
import QtQuick
import QtQuick.Controls

/// A row of the quick switcher or the command palette: a name, with a detail at the right.
AbstractButton {
    id: row

    property string detail
    property bool current: false

    height: Theme.listRowHeight + Theme.space2
    hoverEnabled: true
    Accessible.name: text

    background: Rectangle {
        radius: Theme.radiusSm
        color: row.current ? Theme.tint(Theme.accent, 0.18) : row.hovered ? Theme.surfaceAlt :
                                                                            Theme.surface
    }

    contentItem: Item {
        Text {
            id: name

            anchors {
                left: parent.left
                leftMargin: Theme.space4
                verticalCenter: parent.verticalCenter
            }
            width: Math.min(implicitWidth, parent.width * 0.6)
            text: row.text
            elide: Text.ElideRight
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textBase
            font.weight: Theme.weightStrong
        }
        Text {
            anchors {
                left: name.right
                leftMargin: Theme.space4
                right: parent.right
                rightMargin: Theme.space4
                verticalCenter: parent.verticalCenter
            }
            horizontalAlignment: Text.AlignRight
            text: row.detail
            elide: Text.ElideLeft
            color: Theme.textFaint
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
        }
    }
}
