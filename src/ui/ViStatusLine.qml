pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick

/// Across the foot of the editor while vi mode is on: the mode in caps and in its own color, the
/// file with its kind's icon, then where the cursor is, how far down the view is, and the clock.
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
    /// How far down the view is, as vim says it: Top, Bot, All or a percentage.
    property string place
    /// Lines picked in Visual mode.
    property int pickedLines: 0
    property date now: new Date()

    readonly property color modeFill: mode === "insert" ? Theme.insertMode : mode === "visual"
                                                          ? Theme.visualMode : Theme.accent
    readonly property color modeText: mode === "insert" ? Theme.insertModeText : mode === "visual"
                                                          ? Theme.visualModeText : Theme.accentText
    readonly property color modeEdge: mode === "insert" ? Theme.insertModeEdge : mode === "visual"
                                                          ? Theme.visualModeEdge : Theme.accentEdge
    readonly property string folders: path.lastIndexOf("/") < 0 ? "" : path.slice(0,
                                                                                  path.lastIndexOf(
                                                                                      "/")).split(
                                                                      "/").join(" / ") + " /"
    readonly property string fileName: path.slice(path.lastIndexOf("/") + 1)

    // A rounded sticker on an edge, as the buttons are.
    component Segment: Item {
        id: segment

        default property alias content: row.data
        property color fill: Theme.surfaceAlt
        property color edge: Theme.edge

        implicitWidth: row.implicitWidth + 2 * Theme.space3
        implicitHeight: 26

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: Theme.stickerEdge
            anchors.bottomMargin: -Theme.stickerEdge
            radius: Theme.radiusMd
            color: segment.edge
        }
        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusMd
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

    implicitHeight: 26 + Theme.stickerEdge + 2 * Theme.space2
    color: Theme.surface

    Rectangle {
        width: parent.width
        height: 1
        color: Theme.hairline
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Theme.space2
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -Theme.stickerEdge / 2
        spacing: Theme.space2

        Segment {
            objectName: "viMode"
            fill: root.modeFill
            edge: root.modeEdge

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
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: Theme.space2
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -Theme.stickerEdge / 2
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
            fill: root.modeFill
            edge: root.modeEdge

            Glyph {
                anchors.verticalCenter: parent.verticalCenter
                implicitWidth: 13
                implicitHeight: 13
                name: "clock"
                color: root.modeText
                stroke: Theme.fineGlyphStroke
            }
            Label {
                // The day's short name and the time as the desktop's locale writes them.
                text: Qt.locale().dayName(root.now.getDay(), Locale.ShortFormat) + " "
                      + root.now.toLocaleTimeString(Qt.locale(), Locale.ShortFormat)
                color: root.modeText
                font.weight: Theme.weightHeavy
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
