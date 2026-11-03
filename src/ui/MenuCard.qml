import Jot.Ui
import QtQuick
import QtQuick.Controls

/// A menu in the theme's card style, for MenuEntry items.
Menu {
    id: root

    /// The narrowest the menu gets, whatever its entries.
    property real minimumWidth: 200
    /// The widest of its entries, so none is cut short; Menu itself sizes to its background.
    readonly property real entriesWidth: {
        let widest = 0
        for (let i = 0; i < count; ++i) {
            const entry = itemAt(i)
            if (entry)
                widest = Math.max(widest, entry.implicitWidth)
        }
        return widest + leftPadding + rightPadding
    }

    padding: Theme.space2
    // A submenu's own entry, in the parent menu, looks like the rest.
    delegate: MenuEntry {}

    background: Rectangle {
        implicitWidth: Math.max(root.minimumWidth, root.entriesWidth)
        color: Theme.surface
        border.color: Theme.border
        radius: Theme.radiusLg

        SoftShadow {
            anchors.fill: parent
            radius: parent.radius
        }
    }
}
