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
    /// A command is being typed after `:`, in place of the mode and file.
    property bool commanding: false
    /// What the last command had to say, shown for a while.
    property string message

    /// A command was typed and Return pressed.
    signal run(string command)
    /// The command line was left without running anything.
    signal cancelled

    function openCommand() {
        message = ""
        command.text = ""
        commanding = true
        command.forceActiveFocus()
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
            Keys.onPressed: event => {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
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
            objectName: "viMode"
            fill: root.modeFill

            Label {
                text: root.mode.toUpperCase()
                color: root.modeText
                font.weight: Theme.weightHeavy
                font.letterSpacing: 1.2
            }
        }
        Segment {
            objectName: "viFile"
            visible: root.path !== ""

            Glyph {
                anchors.verticalCenter: parent.verticalCenter
                name: root.kind
                color: root.modeFill
                stroke: Theme.fineGlyphStroke
            }
            Label {
                visible: root.folders !== ""
                text: root.folders
                color: Theme.textFaint
            }
            Label {
                text: root.fileName
            }
        }
        Label {
            objectName: "viPicked"
            visible: root.mode === "visual" && root.pickedLines > 0
            text: root.pickedLines === 1 ? qsTr("1 line") : qsTr("%1 lines").arg(root.pickedLines)
            color: Theme.textMuted
        }
        Label {
            objectName: "viPending"
            visible: root.pending !== ""
            text: root.pending
            color: Theme.textMuted
            font.family: Theme.monoFontFamily
        }
        Label {
            objectName: "viMessage"
            visible: root.message !== ""
            text: root.message
            color: Theme.textMuted
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
            objectName: "viCursor"
            visible: root.showCursor

            Label {
                text: root.line + ":" + root.column
            }
        }
        Segment {
            objectName: "viPlace"
            visible: root.place !== ""

            Label {
                text: root.place
                color: Theme.textMuted
            }
        }
        Segment {
            objectName: "viClock"

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
