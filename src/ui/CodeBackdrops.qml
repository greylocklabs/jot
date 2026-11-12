pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick

/// The raised surface behind each run of code lines a NoteReader draws, laid under the text of
/// the TextEdit that shows it.
Repeater {
    id: root

    /// The TextEdit the reader draws into.
    required property TextEdit view
    /// The reader's `codeRuns`.
    required property var runs
    /// The room the reader leaves around the code.
    required property real padding

    model: runs.length

    Rectangle {
        required property int index

        readonly property var run: root.runs[index] ?? ({})
        readonly property rect firstLine: {
            root.view.width
            root.view.contentHeight
            return root.view.positionToRectangle(run.start ?? 0)
        }
        readonly property rect lastLine: {
            root.view.width
            root.view.contentHeight
            return root.view.positionToRectangle(run.end ?? 0)
        }

        objectName: "codeBackdrop"
        z: -1
        x: root.view.leftPadding
        y: firstLine.y - root.padding
        width: root.view.width - root.view.leftPadding - root.view.rightPadding
        height: lastLine.y + lastLine.height + root.padding - y
        color: Theme.surfaceAlt
    }
}
