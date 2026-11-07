import Jot.Ui
import QtQuick

/// An X that takes a second click to remove what it is on: the first turns it into a trash can in
/// the same place, and moving away puts the X back.
StickerButton {
    id: root

    /// The second click: whatever it is on goes.
    signal confirmed

    glyph: armed ? "trash" : "close"
    // The can's ribs run together at the X's stroke.
    glyphStroke: armed ? 1.4 : 2.2
    destructive: armed
    hoverEnabled: true
    onClicked: {
        if (!armed) {
            armed = true
            return
        }
        armed = false
        confirmed()
    }
    onHoveredChanged: {
        if (!hovered)
            armed = false
    }
    onVisibleChanged: armed = false
}
