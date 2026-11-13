pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick

/// Across the foot of the editor while vi mode is on: the mode in caps and in its own color, the
/// file with its kind's icon, then where the cursor is, how far through the file, and the clock.
/// It is flat, without the edge buttons have, as nothing on it is clicked.
Rectangle {
    id: root

    /// "normal", "insert" or "visual".
    property string mode: "normal"
    /// Keys typed toward a command that is not finished yet.
    property string pending
    /// The file, from the vault's folder.
    property string path
    /// "note", "data" or "table", for the icon.
    property string kind: "note"
    property int line: 1
    property int column: 1
    property bool showCursor: true
    /// How far through the file the cursor is, as a percentage.
    property string place
    /// Lines picked in Visual mode.
    property int pickedLines: 0
    property date now: new Date()
    /// The room beside the mode and the cursor's place, which the rest of the right-hand side
    /// takes while it fits.
    readonly property real spare: width - modeSegment.width - cursorSegment.implicitWidth - 4
                                  * Theme.space2
    /// What else shares the left with the file: lines picked, keys pending and what a command
    /// said, each with its gap.
    readonly property real besideFile: (pickedLabel.visible ? pickedLabel.implicitWidth
                                                              + Theme.space2 : 0) + (
                                           pendingLabel.visible ? pendingLabel.implicitWidth
                                                                  + Theme.space2 : 0) + (
                                           messageLabel.visible ? messageLabel.implicitWidth
                                                                  + Theme.space2 : 0)
    /// A command is being typed after `:`, in place of the mode and file.
    property bool commanding: false
    /// What the last command had to say, shown for a while.
    property string message
    /// The commands there are, to offer as one is typed.
    readonly property var commands: [
        {
            "text": "w",
            "detail": qsTr("Save")
        },
        {
            "text": "q",
            "detail": qsTr("Close the tab")
        },
        {
            "text": "wq",
            "detail": qsTr("Save and close the tab")
        },
        {
            "text": "s/",
            "detail": qsTr("Replace on the line: s/old/new/")
        },
        {
            "text": "%s/",
            "detail": qsTr("Replace in the whole note: %s/old/new/g")
        },
        {
            "text": "noh",
            "detail": qsTr("Clear the find highlights")
        },
        {
            "text": "$",
            "detail": qsTr("Go to the last line")
        },
        {
            "text": "help",
            "detail": qsTr("Vi's keys and commands")
        }
    ]
    /// Those that start with what is typed, which is not one of them already.
    readonly property var offered: commanding ? commands.filter(each => each.text.startsWith(
                                                                            command.text)
                                                                        && each.text
                                                                        !== command.text) : []
    /// The one the arrows chose, or -1.
    property int chosen: -1

    /// A command was typed and Return pressed.
    signal run(string command)
    /// The command line was left without running anything.
    signal cancelled

    function openCommand() {
        message = ""
        command.text = ""
        chosen = -1
        commanding = true
        command.forceActiveFocus()
    }

    /// Puts the command chosen, or the first offered, on the line; says whether one was.
    function take() {
        const each = offered[Math.max(chosen, 0)]
        if (!each)
            return false
        command.text = each.text
        command.cursorPosition = command.text.length
        chosen = -1
        return true
    }

    function say(words) {
        message = words
        if (words !== "")
            messageTimer.restart()
    }

    readonly property color modeFill: mode === "insert" ? Theme.insertMode : mode === "visual"
                                                          ? Theme.visualMode : Theme.accent
    readonly property color modeText: mode === "insert" ? Theme.insertModeText : mode === "visual"
                                                          ? Theme.visualModeText : Theme.accentText
    readonly property string folders: path.lastIndexOf("/") < 0 ? "" : path.slice(0,
                                                                                  path.lastIndexOf(
                                                                                      "/")).split(
                                                                      "/").join(" / ") + " /"
    readonly property string fileName: path.slice(path.lastIndexOf("/") + 1)

    // A run of text; only the mode's has a fill.
    component Segment: Item {
        id: segment

        default property alias content: row.data
        property color fill: "transparent"

        implicitWidth: row.implicitWidth + 2 * Theme.space3
        implicitHeight: 22

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusSm
            color: segment.fill
        }
        Row {
            id: row

            anchors.centerIn: parent
            spacing: 6
        }
    }

    component Label: Text {
        anchors.verticalCenter: parent?.verticalCenter
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textSm
        font.weight: Theme.weightBold
        font.features: {
            "tnum": 1
        }
    }

    implicitHeight: 22 + 2 * Theme.space2
    color: Theme.surface

    Rectangle {
        width: parent.width
        height: 1
        color: Theme.hairline
    }

    // The command being typed, as in vim's command line.
    Row {
        anchors.left: parent.left
        anchors.leftMargin: Theme.space3
        anchors.right: rightSide.left
        anchors.verticalCenter: parent.verticalCenter
        visible: root.commanding
        spacing: 2

        Label {
            text: ":"
            font.family: Theme.monoFontFamily
        }
        TextInput {
            id: command

            objectName: "viCommand"
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - Theme.space5
            color: Theme.text
            selectionColor: Theme.tint(Theme.accent, 0.4)
            font.family: Theme.monoFontFamily
            font.pixelSize: Theme.textSm
            Accessible.name: qsTr("Command")
            onTextEdited: root.chosen = -1
            Keys.onPressed: event => {
                if (event.key === Qt.Key_Down || event.key === Qt.Key_Up) {
                    const count = root.offered.length
                    if (count > 0)
                        root.chosen = event.key === Qt.Key_Down ? (root.chosen + 1) % count : (
                                                                      root.chosen + count - 1)
                                                                  % count
                    event.accepted = true
                } else if (event.key === Qt.Key_Tab) {
                    root.take()
                    event.accepted = true
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    // One chosen with the arrows is taken first; one that needs more typed,
                    // as s/ does, waits for it.
                    if (root.chosen >= 0 && root.take() && text.endsWith("/")) {
                        event.accepted = true
                        return
                    }
                    const typed = text
                    root.commanding = false
                    root.run(typed)
                    event.accepted = true
                } else if (event.key === Qt.Key_Escape || (event.key === Qt.Key_Backspace && text
                                                           === "")) {
                    // Backspace past the start leaves too, as in vim.
                    root.commanding = false
                    root.cancelled()
                    event.accepted = true
                }
            }
            onActiveFocusChanged: {
                if (!activeFocus)
                    root.commanding = false
            }
        }
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Theme.space2
        anchors.verticalCenter: parent.verticalCenter
        visible: !root.commanding
        spacing: Theme.space2

        Segment {
            id: modeSegment

            objectName: "viMode"
            fill: root.modeFill

            Label {
                text: root.mode.toUpperCase()
                color: root.modeText
                font.weight: Theme.weightHeavy
                font.letterSpacing: 1.2
            }
        }
        // As the line narrows, the folders go first, then the name is cut short, then the file
        // goes, leaving the mode and the right-hand side.
        Segment {
            id: fileSegment

            /// What there is for the file between the mode and the right-hand side.
            readonly property real room: root.width - modeSegment.width - rightSide.width - 2
                                         * Theme.space2 - 3 * Theme.space2 - 2 * Theme.space3
                                         - root.besideFile
            readonly property real glyphRoom: fileGlyph.width + 6

            objectName: "viFile"
            visible: root.path !== "" && room >= glyphRoom + 40

            TextMetrics {
                id: folderMetrics
                font: folderLabel.font
                text: root.folders
            }
            TextMetrics {
                id: nameMetrics
                font: nameLabel.font
                text: root.fileName
            }
            Glyph {
                id: fileGlyph
                anchors.verticalCenter: parent.verticalCenter
                name: root.kind
                color: root.modeFill
                stroke: Theme.fineGlyphStroke
            }
            Label {
                id: folderLabel
                objectName: "viFolders"
                visible: root.folders !== "" && fileSegment.room >= fileSegment.glyphRoom
                         + folderMetrics.advanceWidth + 6 + nameMetrics.advanceWidth
                text: root.folders
                color: Theme.textFaint
            }
            Label {
                id: nameLabel
                objectName: "viFileName"
                width: Math.min(nameMetrics.advanceWidth, Math.max(0, fileSegment.room
                                                                   - fileSegment.glyphRoom))
                elide: Text.ElideRight
                text: root.fileName
            }
        }
        Label {
            id: pickedLabel
            objectName: "viPicked"
            visible: root.mode === "visual" && root.pickedLines > 0
            text: root.pickedLines === 1 ? qsTr("1 line") : qsTr("%1 lines").arg(root.pickedLines)
            color: Theme.textMuted
        }
        Label {
            id: pendingLabel
            objectName: "viPending"
            visible: root.pending !== ""
            text: root.pending
            color: Theme.textMuted
            font.family: Theme.monoFontFamily
        }
        Label {
            id: messageLabel
            objectName: "viMessage"
            visible: root.message !== ""
            text: root.message
            color: Theme.textMuted
        }
    }

    // The commands that start with what is typed, above the line.
    Rectangle {
        objectName: "viCommandList"
        visible: root.commanding && root.offered.length > 0
        x: Theme.space3
        y: -height - Theme.space1
        width: 320
        height: list.implicitHeight + 2 * Theme.space2
        radius: Theme.radiusMd
        color: Theme.surface
        border.color: Theme.border

        Column {
            id: list

            anchors.fill: parent
            anchors.margins: Theme.space2

            Repeater {
                model: root.offered

                Rectangle {
                    id: offer

                    required property var modelData
                    required property int index

                    width: list.width
                    height: Theme.listRowHeight
                    radius: Theme.radiusSm
                    color: offer.index === root.chosen ? Theme.tint(Theme.accent, 0.18) :
                                                         "transparent"

                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        x: Theme.space2
                        spacing: Theme.space3

                        Text {
                            text: ":" + offer.modelData.text
                            color: Theme.text
                            font.family: Theme.monoFontFamily
                            font.pixelSize: Theme.textSm
                        }
                        Text {
                            text: offer.modelData.detail
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textSm
                        }
                    }
                }
            }
        }
    }

    Timer {
        id: messageTimer
        interval: 4000
        onTriggered: root.message = ""
    }

    Row {
        id: rightSide

        anchors.right: parent.right
        anchors.rightMargin: Theme.space2
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space2

        Segment {
            id: cursorSegment

            objectName: "viCursor"
            visible: root.showCursor

            Label {
                text: root.line + ":" + root.column
            }
        }
        Segment {
            id: placeSegment

            objectName: "viPlace"
            // On a line too narrow for all, the clock goes, then this.
            visible: root.place !== "" && root.spare >= placeSegment.implicitWidth + Theme.space2

            Label {
                text: root.place
                color: Theme.textMuted
            }
        }
        Segment {
            id: clockSegment

            objectName: "viClock"
            visible: root.spare >= placeSegment.implicitWidth + clockSegment.implicitWidth + 2
                     * Theme.space2

            Glyph {
                anchors.verticalCenter: parent.verticalCenter
                implicitWidth: 13
                implicitHeight: 13
                name: "clock"
                color: root.modeFill
                stroke: Theme.fineGlyphStroke
            }
            Label {
                // The day's short name and the time as the desktop's locale writes them.
                text: Qt.locale().dayName(root.now.getDay(), Locale.ShortFormat) + " "
                      + root.now.toLocaleTimeString(Qt.locale(), Locale.ShortFormat)
                color: Theme.textMuted
            }
        }
    }

    // Ticks often enough that the minute turns over on time.
    Timer {
        interval: 5000
        running: root.visible
        repeat: true
        triggeredOnStart: true
        onTriggered: root.now = new Date()
    }
}
