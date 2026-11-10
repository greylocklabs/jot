pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// The row over a note: Markdown formatting for the selection or the line the cursor is
/// on, and on the right, how it shows, its outline, and whether the note is a task.
Rectangle {
    id: root

    /// The note's front matter, from the styler.
    property var frontMatter: ({})
    /// Whether there is a microphone, and whether it is recording.
    property bool canRecord: false
    property bool recording: false

    signal formatRequested(string action)

    readonly property string task: frontMatter.task || ""

    /// The formatting buttons' width on one line.
    readonly property real formatWidth: {
        let sum = 0
        let shown = 0
        for (const child of row.children) {
            if (child.visible) {
                sum += child.width
                shown += 1
            }
        }
        return sum + Math.max(0, shown - 1) * row.spacing
    }
    readonly property real trailingWidth: modesHolder.width + outlineToggle.implicitWidth
                                          + taskToggle.implicitWidth + 2 * trailing.spacing
    /// Too narrow for the formatting and the switches on one line: the switches go below it,
    /// and each wraps as it must.
    readonly property bool stacked: row.visible && width - 2 * Theme.space5 - Theme.space3
                                    - trailingWidth < formatWidth

    implicitHeight: stacked ? row.height + trailing.height + 3 * Theme.space2 : Math.max(
                                  row.visible ? row.height : 0, trailing.height) + 2 * Theme.space2
    color: Theme.bg

    Rectangle {
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: 1
        color: Theme.hairline
    }

    Flow {
        id: row

        objectName: "formatButtons"
        // Nothing to format while reading.
        visible: !Settings.readMode
        x: Theme.space5
        y: root.stacked ? Theme.space2 : (root.height - height) / 2
        width: root.width - 2 * Theme.space5 - (root.stacked ? 0 : trailing.width + Theme.space3)
        spacing: Theme.space1

        // One button for the three heading sizes, so the row has room for the rest.
        FormatButton {
            id: headings

            label: "H"
            tip: qsTr("Heading")
            onClicked: headingMenu.popup(headings, 0, headings.height + Theme.space1)

            MenuCard {
                id: headingMenu

                minimumWidth: 160

                MenuEntry {
                    text: qsTr("Heading 1   Ctrl+1")
                    onTriggered: root.formatRequested("h1")
                }
                MenuEntry {
                    text: qsTr("Heading 2   Ctrl+2")
                    onTriggered: root.formatRequested("h2")
                }
                MenuEntry {
                    text: qsTr("Heading 3   Ctrl+3")
                    onTriggered: root.formatRequested("h3")
                }
            }
        }
        Divider {}
        FormatButton {
            label: "B"
            bold: true
            format: "bold"
            tip: qsTr("Bold (Ctrl+B)")
        }
        FormatButton {
            label: "I"
            italic: true
            format: "italic"
            tip: qsTr("Italic (Ctrl+I)")
        }
        FormatButton {
            label: "S"
            strike: true
            format: "strike"
            tip: qsTr("Strikethrough (Ctrl+Shift+X)")
        }
        FormatButton {
            label: "<>"
            mono: true
            format: "code"
            tip: qsTr("Code (Ctrl+E)")
        }
        FormatButton {
            label: qsTr("Link")
            underline: true
            format: "link"
            tip: qsTr("Link (Ctrl+K)")
        }
        Divider {}
        FormatButton {
            glyph: "list-bullet"
            format: "bullet"
            tip: qsTr("Bulleted list")
        }
        FormatButton {
            glyph: "list-number"
            format: "numbered"
            tip: qsTr("Numbered list")
        }
        FormatButton {
            glyph: "task"
            format: "task"
            tip: qsTr("Checkbox (Ctrl+Enter)")
        }
        FormatButton {
            glyph: "quote"
            format: "quote"
            tip: qsTr("Quote")
        }
        FormatButton {
            glyph: "table"
            format: "table"
            tip: qsTr("Table, or line up the one here")
        }
        FormatButton {
            label: "{ }"
            mono: true
            format: "codeblock"
            tip: qsTr("Code block")
        }
        // Records from the microphone into the note, where there is one.
        WhyDisabled {
            target: recordButton
            reason: qsTr("No microphone found.")

            FormatButton {
                id: recordButton

                objectName: "recordButton"
                enabled: root.canRecord
                label: root.recording ? qsTr("Stop") : qsTr("Rec")
                format: "record"
                tip: root.recording ? qsTr("Stop recording") : qsTr("Record from the microphone")
            }
        }
    }

    Flow {
        id: trailing

        objectName: "formatTrailing"
        x: root.width - Theme.space5 - width
        y: root.stacked ? row.y + row.height + Theme.space2 : (root.height - height) / 2
        width: Math.min(root.trailingWidth, root.width - 2 * Theme.space5)
        spacing: Theme.space3

        // The note styled as it is typed, its Markdown as written, or drawn to read.
        Item {
            id: modesHolder

            width: modes.implicitWidth
            // As tall as the toggles beside it, so the line lines up.
            height: Math.max(modes.implicitHeight, outlineToggle.implicitHeight)

            ModeSwitch {
                id: modes

                objectName: "modes"
                anchors.verticalCenter: parent.verticalCenter
                options: [
                    {
                        "label": qsTr("Styled"),
                        "tip": qsTr("Style the Markdown as you type (Ctrl+/)")
                    },
                    {
                        "label": qsTr("Source"),
                        "tip": qsTr("Show the Markdown as written (Ctrl+/)")
                    },
                    {
                        "label": qsTr("Read"),
                        "tip": qsTr(
                                   "Read the note as it looks, with links to follow (Ctrl+Shift+R)")
                    }
                ]
                current: Settings.readMode ? 2 : Settings.sourceMode ? 1 : 0
                onPicked: index => {
                    Settings.readMode = index === 2
                    if (index < 2)
                        Settings.sourceMode = index === 1
                }
            }
        }

        // The note's headings down the side.
        Toggle {
            id: outlineToggle

            objectName: "outlineToggle"
            text: qsTr("Outline")
            checked: Settings.outlineShown
            focusPolicy: Qt.NoFocus
            Accessible.description: qsTr("Show the note's headings beside it (Ctrl+Shift+O)")
            onToggled: Settings.outlineShown = checked
        }

        // A note that is a task shows in Tasks until it is done.
        Toggle {
            id: taskToggle

            text: qsTr("Task")
            // Follows the note: a click asks for the change, and the text says what it is.
            checked: root.task !== ""
            focusPolicy: Qt.NoFocus
            Accessible.description: checked ? qsTr("No longer a task") : qsTr(
                                                  "Make this note a task")
            onToggled: {
                checked = Qt.binding(() => root.task !== "")
                root.formatRequested("taskNote")
            }
        }
    }

    /// A small button that formats: a label drawn in the style it gives, or a glyph.
    component FormatButton: AbstractButton {
        id: button

        property string label
        property string glyph
        property string format
        property string tip
        property bool bold
        property bool italic
        property bool strike
        property bool underline
        property bool mono

        implicitWidth: Math.max(implicitHeight, caption.implicitWidth + 2 * Theme.space2)
        // As tall as the pills beside it, so the row lines up.
        implicitHeight: 32
        focusPolicy: Qt.NoFocus
        Accessible.name: tip
        onClicked: {
            if (format !== "")
                root.formatRequested(format)
        }

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: button.pressed ? Theme.border : button.hovered ? Theme.surfaceAlt : Theme.tint(
                                                                        Theme.surfaceAlt, 0)
        }
        contentItem: Item {
            Text {
                id: caption
                anchors.centerIn: parent
                visible: button.glyph === ""
                text: button.label
                color: button.hovered ? Theme.text : Theme.textMuted
                font.family: button.mono ? Theme.monoFontFamily : Theme.fontFamily
                font.pixelSize: Theme.textMd
                font.weight: button.bold ? Theme.weightHeavy : Theme.weightBold
                font.italic: button.italic
                font.strikeout: button.strike
                font.underline: button.underline
            }
            Glyph {
                anchors.centerIn: parent
                visible: button.glyph !== ""
                name: button.glyph
                color: caption.color
                stroke: Theme.fineGlyphStroke
            }
        }

        Loader {
            active: button.tip !== "" && button.hovered

            sourceComponent: Tip {
                parent: button
                visible: !button.pressed
                text: button.tip
                delay: Theme.durSlow * 2
                closePolicy: Popup.NoAutoClose
            }
        }
    }

    component Divider: Item {
        width: Theme.space3
        height: 32

        Rectangle {
            anchors.centerIn: parent
            width: 1
            height: 18
            color: Theme.border
        }
    }
}
