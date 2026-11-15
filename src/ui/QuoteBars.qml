pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick

/// The bar down the left of each level of a quote a NoteReader draws, laid under the text of the
/// TextEdit that shows it.
Repeater {
    id: root

    /// The TextEdit the reader draws into.
    required property TextEdit view
    /// The reader's `quoteRuns`.
    required property var runs
    /// How much further in each level's text starts.
    required property real indent

    model: runs.length

    Rectangle {
        required property int index

        readonly property var run: root.runs[index] ?? ({})
        readonly property rect firstLine: {
            root.view.width
            root.view.contentHeight
            return root.view.positionToRectangle(Math.min(run.start ?? 0, root.view.length))
        }
        readonly property rect lastLine: {
            root.view.width
            root.view.contentHeight
            return root.view.positionToRectangle(Math.min(run.end ?? 0, root.view.length))
        }

        objectName: "quoteBar"
        z: -1
        x: root.view.leftPadding + ((run.level ?? 1) - 1) * root.indent
        y: firstLine.y
        width: Theme.space2
        height: lastLine.y + lastLine.height - y
        radius: width / 2
        color: Theme.border
    }
}
