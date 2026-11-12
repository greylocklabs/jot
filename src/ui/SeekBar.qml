pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// Where a sound or a video is, to drag to another place: a thin track filled to here in the
/// accent, with a round handle, in the theme's colors.
Slider {
    id: root

    implicitHeight: 24

    background: Rectangle {
        x: root.leftPadding
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: root.availableWidth
        height: 4
        radius: height / 2
        color: Theme.border

        Rectangle {
            width: root.visualPosition * parent.width
            height: parent.height
            radius: parent.radius
            color: root.enabled ? Theme.accent : Theme.textFaint
        }
    }
    handle: Rectangle {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: root.pressed || root.hovered ? 16 : 12
        height: width
        radius: width / 2
        color: root.enabled ? Theme.accent : Theme.textFaint
        border.width: root.visualFocus ? 2 : 0
        border.color: Theme.text

        Behavior on width {
            NumberAnimation {
                duration: Theme.durFast
            }
        }
    }
}
