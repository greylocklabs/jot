pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// Holds a control and, while it is disabled, says why on hover; a disabled control gets no
/// hover of its own. The control goes inside, named as `target`.
Item {
    id: root

    property Item target
    /// Why the control cannot be used now.
    property string reason

    implicitWidth: target ? target.width : 0
    implicitHeight: target ? target.height : 0

    HoverHandler {
        id: hover
        enabled: root.target !== null && !root.target.enabled && root.reason !== ""
    }

    Loader {
        active: hover.enabled && hover.hovered

        sourceComponent: Tip {
            parent: root
            visible: true
            text: root.reason
            delay: Theme.durSlow * 2
            closePolicy: Popup.NoAutoClose
        }
    }
}
