pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// An arrow left of each heading of a TextEdit that has something under it, which folds that
/// away or shows it again. It shows while the pointer is on the heading's line, and always on a
/// folded one, which also marks that text is hidden after it.
Repeater {
    id: root

    /// The TextEdit the headings are in.
    required property TextEdit view
    /// The styler's or the reader's `folds`.
    required property var folds
    /// How far left of the text the arrow sits, clear of anything else in the margin.
    property real inset: Theme.space2

    /// The heading named `key` should be folded, or shown again.
    signal toggled(string key, bool fold)

    model: folds.length

    Item {
        id: line

        required property int index
        readonly property var fold: root.folds[index] ?? ({})
        readonly property rect start: {
            root.view.width
            root.view.contentHeight
            return root.view.positionToRectangle(Math.min(fold.position ?? 0, root.view.length))
        }
        readonly property rect end: {
            root.view.width
            root.view.contentHeight
            return root.view.positionToRectangle(Math.min(fold.end ?? 0, root.view.length))
        }

        objectName: "foldLine"
        y: start.y
        width: root.view.width
        height: end.y + end.height - start.y

        HoverHandler {
            id: lineHover
        }

        AbstractButton {
            id: arrow

            objectName: "foldArrow"
            x: -width - root.inset
            y: (line.start.height - height) / 2
            width: Theme.smallGlyphSize + Theme.space3
            height: Theme.smallGlyphSize + Theme.space3
            opacity: lineHover.hovered || hovered || line.fold.folded ? 1 : 0
            focusPolicy: Qt.NoFocus
            Accessible.name: line.fold.folded ? qsTr("Show what is under %1").arg(line.fold.key) :
                                                qsTr("Fold away what is under %1").arg(
                                                    line.fold.key)
            onClicked: root.toggled(line.fold.key, !line.fold.folded)

            HoverHandler {
                cursorShape: Qt.PointingHandCursor
            }

            contentItem: Glyph {
                name: line.fold.folded ? "chevron-right" : "chevron-down"
                color: arrow.hovered ? Theme.text : Theme.textFaint
                stroke: Theme.smallGlyphStroke
                implicitWidth: Theme.smallGlyphSize
                implicitHeight: Theme.smallGlyphSize
            }
            background: null
        }

        // Marks that something is folded away under the heading; a click shows it again.
        AbstractButton {
            objectName: "foldMark"
            visible: line.fold.folded === true
            x: line.end.x + Theme.space3
            y: line.end.y - line.start.y + (line.end.height - height) / 2
            focusPolicy: Qt.NoFocus
            Accessible.name: qsTr("Show what is under %1").arg(line.fold.key)
            onClicked: root.toggled(line.fold.key, false)
            padding: Theme.space1
            leftPadding: Theme.space3
            rightPadding: Theme.space3

            HoverHandler {
                cursorShape: Qt.PointingHandCursor
            }

            contentItem: Text {
                text: "…"
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSm
                font.weight: Theme.weightBold
            }
            background: Rectangle {
                radius: height / 2
                color: Theme.surfaceAlt
            }
        }
    }
}
