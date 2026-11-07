import Jot.Ui
import QtQuick

/// An X that takes a second click to remove what it is on: the first turns it into a trash can in
/// the same place, and moving away puts the X back.
StickerButton {
    id: root

    /// The second click: whatever it is on goes.
    signal confirmed

    // Two quick clicks come as a double click, and the second may or may not also count as a
    // click, depending on the Qt version; it is taken once either way.
    property bool tookDoubleClick: false

    glyph: armed ? "trash" : "close"
    // The can's ribs run together at the X's stroke.
    glyphStroke: armed ? 1.4 : 2.2
    destructive: armed
    hoverEnabled: true
    function take() {
        if (!armed) {
            armed = true
            return
        }
        armed = false
        confirmed()
    }

    onPressed: tookDoubleClick = false
    onDoubleClicked: {
        tookDoubleClick = true
        take()
    }
    onClicked: {
        if (!tookDoubleClick)
            take()
    }
    onHoveredChanged: {
        if (!hovered)
            armed = false
    }
    onVisibleChanged: armed = false
}
