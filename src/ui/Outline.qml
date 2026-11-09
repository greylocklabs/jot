pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// The note's headings down the side of the editor, each going to its place, with the one the
/// cursor is under marked.
Item {
    id: root

    /// `{text, level, position}` for each heading, as MarkdownStyler gives them.
    property var headings: []
    /// The heading the cursor is under, or -1.
    property int current: -1

    signal chosen(int index)

    implicitWidth: 240

    Rectangle {
        width: 1
        height: parent.height
        color: Theme.hairline
    }

    Text {
        id: title

        x: Theme.space4
        y: Theme.space4
        text: qsTr("Outline")
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textXs
        font.weight: Theme.weightBold
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 0.6
    }

    Text {
        x: Theme.space4
        anchors.top: title.bottom
        anchors.topMargin: Theme.space3
        width: parent.width - 2 * Theme.space4
        visible: root.headings.length === 0
        wrapMode: Text.Wrap
        text: qsTr("Headings in the note show here.")
        color: Theme.textFaint
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textSm
    }

    ListView {
        id: list

        objectName: "outlineList"
        anchors {
            top: title.bottom
            topMargin: Theme.space2
            left: parent.left
            leftMargin: Theme.space2
            right: parent.right
            rightMargin: Theme.space2
            bottom: parent.bottom
        }
        clip: true
        model: root.headings
        boundsBehavior: Flickable.StopAtBounds
        currentIndex: root.current
        ScrollBar.vertical: ScrollBar {}

        delegate: Item {
            id: row

            required property var modelData
            required property int index

            objectName: "outlineEntry"
            width: ListView.view.width
            height: Theme.listRowHeight
            Accessible.role: Accessible.Link
            Accessible.name: modelData.text

            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusMd
                color: row.index === root.current ? Theme.tint(Theme.accent, 0.18) :
                                                    rowHover.hovered ? Theme.surfaceAlt :
                                                                       "transparent"
            }
            Text {
                anchors.fill: parent
                leftPadding: Theme.space2 + (row.modelData.level - 1) * Theme.space4
                rightPadding: Theme.space2
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                textFormat: Text.PlainText
                text: row.modelData.text
                color: row.index === root.current ? Theme.text : Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSm
                font.weight: row.modelData.level === 1 ? Theme.weightBold : Theme.weightStrong
            }
            HoverHandler {
                id: rowHover
                cursorShape: Qt.PointingHandCursor
            }
            TapHandler {
                onTapped: root.chosen(row.index)
            }
        }
    }
}
