pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// The bar over a note, in two rows: Markdown formatting for the selection or the line the
/// cursor is on; then how it shows, its outline, and at the end, deleting it. A data file, and a
/// note being read, have only the second.
Rectangle {
    id: root

    /// `words`, and the shortcut for `id` after them when it has one, as chosen in Settings.
    function withKey(words, id) {
        shortcuts.actions
        const key = shortcuts.keyText(id)
        return key === "" ? words : qsTr("%1 (%2)").arg(words).arg(key)
    }

    KeyRouter {
        id: shortcuts
    }

    /// Whether there is a microphone, and whether it is recording.
    property bool canRecord: false
    property bool recording: false
    /// A data file shows as a table in Styled and Read, when it reads as one, and as its text in
    /// Source; `whyNotTable` says why it cannot be a table.
    property bool dataFile: false
    property bool tableShown: false
    property bool tableReady: false
    property string whyNotTable

    signal formatRequested(string action)
    /// A mode was picked in the switch, whether or not it changed.
    signal modePicked
    /// Delete was pressed twice, to move the note to the trash.
    signal trashRequested

    /// The width the formatting buttons have, the more button included.
    readonly property real room: width - 2 * Theme.space5
    /// How many of the row's items, from the left, it shows; the rest are in the more menu.
    readonly property int fitting: {
        const items = row.children
        let total = 0
        for (let i = 0; i < items.length; ++i)
            total += items[i].width + (i > 0 ? row.spacing : 0)
        if (total <= room)
            return items.length
        let used = 0
        let count = 0
        for (let i = 0; i < items.length; ++i) {
            const next = used + items[i].width + (i > 0 ? row.spacing : 0)
            if (next > room - more.implicitWidth - row.spacing)
                break
            used = next
            count += 1
        }
        // A divider with nothing after it is not shown.
        while (count > 0 && items[count - 1].menuItems.length === 0)
            count -= 1
        return count
    }
    /// What the more menu offers: everything in the items not shown.
    readonly property var overflow: {
        const out = []
        for (let i = fitting; i < row.children.length; ++i)
            out.push(...row.children[i].menuItems)
        return out
    }

    implicitHeight: (row.visible ? row.height + Theme.space2 : 0) + trailing.height + 2
                    * Theme.space2
    Component.onCompleted: {
        for (let i = 0; i < row.children.length; ++i) {
            const at = i
            row.children[i].visible = Qt.binding(() => at < root.fitting)
        }
    }
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

    // The formatting, on one line; what does not fit is in the more menu at its end.
    Row {
        id: row

        objectName: "formatButtons"
        // Nothing to format while reading, in a data file, or in a vault only read.
        visible: !Settings.readMode && !root.dataFile && Vault.writable
        x: Theme.space5
        y: Theme.space2
        spacing: Theme.space1

        // One button for the three heading sizes, so the row has room for the rest.
        FormatButton {
            id: headings

            label: "H"
            tip: qsTr("Heading")
            menuItems: [
                {
                    "text": qsTr("Heading 1   Ctrl+1"),
                    "format": "h1"
                },
                {
                    "text": qsTr("Heading 2   Ctrl+2"),
                    "format": "h2"
                },
                {
                    "text": qsTr("Heading 3   Ctrl+3"),
                    "format": "h3"
                }
            ]
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
            tip: root.withKey(qsTr("Bold"), "bold")
        }
        FormatButton {
            label: "I"
            italic: true
            format: "italic"
            tip: root.withKey(qsTr("Italic"), "italic")
        }
        FormatButton {
            label: "S"
            strike: true
            format: "strike"
            tip: root.withKey(qsTr("Strikethrough"), "strike")
        }
        FormatButton {
            label: "<>"
            mono: true
            format: "code"
            tip: root.withKey(qsTr("Code"), "code")
        }
        FormatButton {
            label: qsTr("Link")
            underline: true
            format: "link"
            tip: root.withKey(qsTr("Link"), "link")
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
            tip: root.withKey(qsTr("Checkbox"), "checkbox")
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
            readonly property var menuItems: recordButton.menuItems

            target: recordButton
            reason: qsTr("No microphone found.")

            FormatButton {
                id: recordButton

                objectName: "recordButton"
                enabled: root.canRecord
                glyph: root.recording ? "stop" : "mic"
                format: "record"
                tip: root.recording ? qsTr("Stop recording") : qsTr("Record from the microphone")
            }
        }
    }

    FormatButton {
        id: more

        objectName: "formatMore"
        visible: row.visible && root.overflow.length > 0
        x: row.x + row.width + row.spacing
        y: row.y
        glyph: "more"
        tip: qsTr("More formatting")
        onClicked: overflowMenu.popup(more, 0, more.height + Theme.space1)

        MenuCard {
            id: overflowMenu

            objectName: "formatOverflow"
            minimumWidth: 200

            Repeater {
                model: root.overflow

                MenuEntry {
                    required property var modelData
                    text: modelData.text
                    enabled: modelData.enabled !== false
                    onTriggered: root.formatRequested(modelData.format)
                }
            }
        }
    }

    // How the note shows and its outline, then deleting it, at the far end.
    Item {
        id: trailing

        objectName: "formatTrailing"
        x: Theme.space5
        y: row.visible ? row.y + row.height + Theme.space2 : Theme.space2
        width: root.width - 2 * Theme.space5
        height: Math.max(modes.implicitHeight, outlineToggle.implicitHeight, trash.implicitHeight)

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space3

            // The note styled as it is typed, its Markdown as written, or drawn to read; for a
            // data file, its records as a table to edit or read, or its text.
            ModeSwitch {
                id: modes

                readonly property string notTable: qsTr("This can't be a table: %1.").arg(
                                                       root.whyNotTable)

                objectName: root.dataFile ? "viewSwitch" : "modes"
                anchors.verticalCenter: parent.verticalCenter
                options: root.dataFile ? [
                                             {
                                                 "label": qsTr("Styled"),
                                                 "tip": root.tableReady ? root.withKey(qsTr(
                                                                                           "Edit the records as a table"),
                                                                                       "sourceMode") :
                                                                          notTable,
                                                 "enabled": root.tableReady
                                             },
                                             {
                                                 "label": qsTr("Source"),
                                                 "tip": root.withKey(qsTr("Edit the file's text"),
                                                                     "sourceMode")
                                             },
                                             {
                                                 "label": qsTr("Read"),
                                                 "tip": root.tableReady ? root.withKey(qsTr(
                                                                                           "Read the records as a table, without changing them"),
                                                                                       "readMode") :
                                                                          notTable,
                                                 "enabled": root.tableReady
                                             }
                                         ] : [
                                             {
                                                 "label": qsTr("Styled"),
                                                 "tip": root.withKey(qsTr(
                                                                         "Style the Markdown as you type"),
                                                                     "sourceMode")
                                             },
                                             {
                                                 "label": qsTr("Source"),
                                                 "tip": root.withKey(qsTr(
                                                                         "Show the Markdown as written"),
                                                                     "sourceMode")
                                             },
                                             {
                                                 "label": qsTr("Read"),
                                                 "tip": root.withKey(qsTr(
                                                                         "Read the note as it looks, with links to follow"),
                                                                     "readMode")
                                             }
                                         ]
                current: root.dataFile ? (!root.tableShown ? 1 : Settings.readMode ? 2 : 0) :
                                         Settings.readMode ? 2 : Settings.sourceMode ? 1 : 0
                onPicked: index => {
                    Settings.readMode = index === 2
                    if (index < 2)
                        Settings.sourceMode = index === 1
                    root.modePicked()
                }
            }

            // The note's headings down the side.
            Toggle {
                id: outlineToggle

                objectName: "outlineToggle"
                anchors.verticalCenter: parent.verticalCenter
                visible: !root.dataFile
                text: qsTr("Outline")
                checked: Settings.outlineShown
                focusPolicy: Qt.NoFocus
                Accessible.description: root.withKey(qsTr("Show the note's headings beside it"),
                                                     "outline")
                onToggled: Settings.outlineShown = checked
            }
            Toggle {
                id: linesToggle

                objectName: "linesToggle"
                anchors.verticalCenter: parent.verticalCenter
                visible: !root.dataFile && !Settings.readMode
                text: qsTr("Lines")
                checked: Settings.lineNumbers
                focusPolicy: Qt.NoFocus
                Accessible.description: root.withKey(qsTr("Show line numbers beside the note"),
                                                     "lineNumbers")
                onToggled: Settings.lineNumbers = checked
            }
        }

        DangerButton {
            id: trash

            objectName: "trash"
            anchors {
                right: parent.right
                verticalCenter: parent.verticalCenter
            }
            visible: Vault.writable
            label: qsTr("Delete")
            armedLabel: qsTr("Move to trash?")
            Accessible.name: armed ? qsTr("Move to trash?") : qsTr("Delete")
            onConfirmed: root.trashRequested()
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
        /// What the more menu offers for it when it does not fit: `{text, format, enabled}`.
        property var menuItems: format !== "" ? [
                                                    {
                                                        "text": tip,
                                                        "format": format,
                                                        "enabled": enabled
                                                    }
                                                ] : []

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
        readonly property var menuItems: []

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
