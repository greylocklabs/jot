pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick

/// What a search found in a NoteReader's note, marked behind the text of the TextEdit that shows
/// it, the match the search is on more strongly.
Repeater {
    id: root

    /// The TextEdit the reader draws into.
    required property TextEdit view
    /// The reader's `found`.
    required property var matches

    model: matches.length

    Rectangle {
        required property int index

        readonly property var match: root.matches[index] ?? ({})
        readonly property rect from: {
            root.view.width
            root.view.contentHeight
            return root.view.positionToRectangle(Math.min(match.start ?? 0, root.view.length))
        }
        readonly property rect to: {
            root.view.width
            root.view.contentHeight
            return root.view.positionToRectangle(Math.min(match.end ?? 0, root.view.length))
        }

        objectName: "foundMark"
        z: -1
        x: from.x
        y: from.y
        // A match that wraps is marked to the end of its first line.
        width: (to.y === from.y ? to.x : root.view.width - root.view.rightPadding) - from.x
        height: from.height
        radius: Theme.radiusSm
        color: Theme.tint(Theme.accent, match.current ? 0.6 : 0.25)
    }
}
