pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick

/// A small line icon drawn from rounded bars, so icons need no image files and
/// take the text color: chevron-left, chevron-right, chevron-down, minimize, maximize,
/// restore, close, check, plus, refresh, bell, search, settings, menu, video, more,
/// list-bullet, list-number, task, quote, table, pin, trash, note, braces, clock, mic, stop, play,
/// pause, sound, muted, sidebar or flag.
Item {
    id: root

    property string name
    property color color: Theme.text
    property real stroke: 2

    implicitWidth: 14
    implicitHeight: 14

    // Each bar is [length, angle, x, y], with sizes as fractions of the icon.
    readonly property var bars: {
        switch (name) {
        case "minimize":
            return [[0.62, 0, 0.5, 0.5]]
        case "close":
            return [[0.74, 45, 0.5, 0.5], [0.74, -45, 0.5, 0.5]]
        case "check":
            return [[0.34, 45, 0.33, 0.6], [0.62, -50, 0.6, 0.47]]
        case "video":
            // The lens: a wedge opening to the right of the body.
            return [[0.3, -38, 0.78, 0.4], [0.3, 38, 0.78, 0.6], [0.34, 90, 0.92, 0.5]]
        case "plus":
            return [[0.7, 0, 0.5, 0.5], [0.7, 90, 0.5, 0.5]]
        case "menu":
            return [[0.8, 0, 0.5, 0.25], [0.8, 0, 0.5, 0.5], [0.8, 0, 0.5, 0.75]]
        case "settings":
            // Three sliders, each with its knob at a different setting.
            return [[0.84, 0, 0.5, 0.22], [0.84, 0, 0.5, 0.5], [0.84, 0, 0.5, 0.78], [0.3, 90, 0.32,
                                                                                      0.22], [0.3,
                                                                                              90, 0.68, 0.5],
                    [0.3, 90, 0.42, 0.78]]
        case "list-bullet":
            return [[0.12, 0, 0.12, 0.22], [0.12, 0, 0.12, 0.5], [0.12, 0, 0.12, 0.78], [0.6, 0,
                                                                                         0.62, 0.22],
                    [0.6, 0, 0.62, 0.5], [0.6, 0, 0.62, 0.78]]
        case "list-number":
            // Upright ticks read as the numbers.
            return [[0.24, 90, 0.12, 0.22], [0.24, 90, 0.12, 0.5], [0.24, 90, 0.12, 0.78], [0.6, 0, 0.62,
                                                                                            0.22], [0.6,
                                                                                                    0, 0.62, 0.5],
                    [0.6, 0, 0.62, 0.78]]
        case "task":
            return [[0.76, 0, 0.5, 0.12], [0.76, 0, 0.5, 0.88], [0.76, 90, 0.12, 0.5], [0.76, 90,
                                                                                        0.88, 0.5],
                    [0.24, 45, 0.37, 0.55], [0.4, -50, 0.57, 0.46]]
        case "sidebar":
            // A window with a column down its left.
            return [[0.76, 0, 0.5, 0.12], [0.76, 0, 0.5, 0.88], [0.76, 90, 0.12, 0.5], [0.76, 90,
                                                                                        0.88, 0.5],
                    [0.76, 90, 0.38, 0.5]]
        case "table":
            return [[0.76, 0, 0.5, 0.12], [0.76, 0, 0.5, 0.88], [0.76, 90, 0.12, 0.5], [0.76, 90,
                                                                                        0.88, 0.5],
                    [0.76, 0, 0.5, 0.5], [0.76, 90, 0.5, 0.5]]
        case "quote":
            return [[0.8, 90, 0.14, 0.5], [0.56, 0, 0.64, 0.22], [0.56, 0, 0.64, 0.5], [0.4, 0, 0.56,
                                                                                        0.78]]
        case "trash":
            // A can: handle, lid, sides, base and two ribs.
            return [[0.28, 0, 0.5, 0.1], [0.84, 0, 0.5, 0.24], [0.66, 90, 0.24, 0.58], [0.66, 90,
                                                                                        0.76, 0.58],
                    [0.52, 0, 0.5, 0.91], [0.42, 90, 0.42, 0.6], [0.42, 90, 0.58, 0.6]]
        case "pin":
            // A push pin: its head, its body, the wide collar and the point.
            return [[0.44, 0, 0.5, 0.1], [0.36, 90, 0.36, 0.3], [0.36, 90, 0.64, 0.3], [0.8, 0, 0.5,
                                                                                        0.5], [0.4,
                                                                                               90, 0.5, 0.74]]
        case "note":
            // A page with its corner turned down, and two lines of writing.
            return [[0.84, 90, 0.22, 0.5], [0.56, 0, 0.5, 0.92], [0.62, 90, 0.78, 0.61], [0.36, 0,
                                                                                          0.4, 0.08],
                    [0.3, 48, 0.68, 0.19], [0.3, 0, 0.5, 0.56], [0.3, 0, 0.5, 0.74]]
        default:
            return []
        }
    }

    // Icons drawn from bars.
    Loader {
        anchors.fill: parent
        active: root.bars.length > 0

        sourceComponent: Item {
            Repeater {
                model: root.bars

                Rectangle {
                    required property var modelData

                    width: root.width * modelData[0]
                    height: root.stroke
                    radius: root.stroke / 2
                    color: root.color
                    antialiasing: true
                    x: root.width * modelData[2] - width / 2
                    y: root.height * modelData[3] - height / 2
                    rotation: modelData[1]
                }
            }
        }
    }

    // The rest have shapes of their own; only the one shown is made.
    Loader {
        anchors.fill: parent
        sourceComponent: ({
                              "bell": bellShape,
                              "chevron-left": chevronShape,
                              "chevron-right": chevronShape,
                              "chevron-down": chevronShape,
                              "chevron-up": chevronShape,
                              "refresh": refreshShape,
                              "more": moreShape,
                              "search": searchShape,
                              "video": videoShape,
                              "braces": bracesShape,
                              "clock": clockShape,
                              "mic": micShape,
                              "stop": stopShape,
                              "play": mediaShape,
                              "pause": mediaShape,
                              "sound": mediaShape,
                              "muted": mediaShape,
                              "flag": flagShape,
                              "maximize": frameShape,
                              "restore": frameShape
                          })[root.name] ?? null
    }

    // A bell: a dome flaring to a rim, with the clapper below.
    Component {
        id: bellShape

        Item {
            Canvas {
                id: bell
                anchors.fill: parent
                antialiasing: true
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    const w = width, h = height, s = root.stroke
                    ctx.strokeStyle = root.color
                    ctx.fillStyle = root.color
                    ctx.lineWidth = s
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(w * 0.14, h * 0.74)
                    ctx.quadraticCurveTo(w * 0.26, h * 0.62, w * 0.26, h * 0.44)
                    ctx.arc(w * 0.5, h * 0.44, w * 0.24, Math.PI, 0)
                    ctx.quadraticCurveTo(w * 0.74, h * 0.62, w * 0.86, h * 0.74)
                    ctx.closePath()
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.arc(w * 0.5, h * 0.86, s * 0.9, 0, 2 * Math.PI)
                    ctx.fill()
                }

                Connections {
                    target: root
                    function onColorChanged() {
                        bell.requestPaint()
                    }
                }
            }
        }
    }

    // A chevron as one line with a round join, so its tip has no seam where two bars meet.
    Component {
        id: chevronShape

        Canvas {
            id: chevron
            antialiasing: true
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                // On a 24-unit grid inset by half the stroke, so nothing pokes out, centered in
                // whatever box the glyph is given.
                const size = Math.min(width, height)
                const unit = (size - root.stroke) / 24
                const atX = v => (width - size) / 2 + root.stroke / 2 + v * unit
                const atY = v => (height - size) / 2 + root.stroke / 2 + v * unit
                const points = root.name === "chevron-up" ? [[6, 15], [12, 9], [18, 15]] :
                                                            root.name === "chevron-down" ? [[6, 9],
                                                                                            [12, 15], [18,
                                                                                                       9]] : root.name
                                                                                           === "chevron-left"
                                                                                           ? [[15, 6],
                                                                                              [9, 12], [15,
                                                                                                        18]] : [[9,
                                                                                                                 6], [15, 12],
                                                                                                                [9, 18]]
                ctx.strokeStyle = root.color
                ctx.lineWidth = root.stroke
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.beginPath()
                ctx.moveTo(atX(points[0][0]), atY(points[0][1]))
                ctx.lineTo(atX(points[1][0]), atY(points[1][1]))
                ctx.lineTo(atX(points[2][0]), atY(points[2][1]))
                ctx.stroke()
            }

            Connections {
                target: root
                function onColorChanged() {
                    chevron.requestPaint()
                }
                function onStrokeChanged() {
                    chevron.requestPaint()
                }
                function onNameChanged() {
                    chevron.requestPaint()
                }
            }
        }
    }

    // Most of a circle with an arrowhead at its open end, for refresh.
    Component {
        id: refreshShape

        Item {
            Canvas {
                id: arc
                anchors.fill: parent
                antialiasing: true
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    // On a 24-unit grid inset by half the stroke, so nothing pokes out.
                    const unit = (width - root.stroke) / 24
                    const at = v => root.stroke / 2 + v * unit
                    ctx.strokeStyle = root.color
                    ctx.lineWidth = root.stroke
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    // Round from the right, open at the upper right.
                    ctx.beginPath()
                    ctx.arc(at(12), at(12), 9 * unit, 0, Math.PI * 1.78)
                    ctx.stroke()
                    // A corner for the head, inside the circle's box.
                    ctx.beginPath()
                    ctx.moveTo(at(21), at(3))
                    ctx.lineTo(at(21), at(9))
                    ctx.lineTo(at(15), at(9))
                    ctx.stroke()
                }

                Connections {
                    target: root
                    function onColorChanged() {
                        arc.requestPaint()
                    }
                    function onStrokeChanged() {
                        arc.requestPaint()
                    }
                }
            }
        }
    }

    // More: three dots in a row.
    Component {
        id: moreShape

        Item {
            Repeater {
                model: root.name === "more" ? [0.2, 0.5, 0.8] : []

                Rectangle {
                    required property real modelData

                    width: root.stroke * 1.5
                    height: width
                    radius: width / 2
                    color: root.color
                    antialiasing: true
                    x: root.width * modelData - width / 2
                    y: root.height / 2 - height / 2
                }
            }
        }
    }

    // Search: a lens with a handle.
    Component {
        id: searchShape

        Item {
            Rectangle {
                x: root.width * 0.08
                y: root.height * 0.08
                width: root.width * 0.6
                height: width
                radius: width / 2
                color: "transparent"
                border.width: root.stroke
                border.color: root.color
                antialiasing: true
            }
            Rectangle {
                width: root.width * 0.34
                height: root.stroke
                radius: root.stroke / 2
                color: root.color
                antialiasing: true
                rotation: 45
                x: root.width * 0.76 - width / 2
                y: root.height * 0.76 - height / 2
            }
        }
    }

    // A pair of curly braces, for a data file.
    Component {
        id: bracesShape

        Canvas {
            id: braces
            antialiasing: true
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                const w = width, h = height
                ctx.strokeStyle = root.color
                ctx.lineWidth = root.stroke
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                for (const side of [-1, 1]) {
                    // Drawn for the left brace and mirrored for the right.
                    const x = f => w * (0.5 + side * (0.5 - f))
                    ctx.beginPath()
                    ctx.moveTo(x(0.36), h * 0.1)
                    ctx.quadraticCurveTo(x(0.24), h * 0.1, x(0.24), h * 0.24)
                    ctx.lineTo(x(0.24), h * 0.38)
                    ctx.quadraticCurveTo(x(0.24), h * 0.5, x(0.1), h * 0.5)
                    ctx.quadraticCurveTo(x(0.24), h * 0.5, x(0.24), h * 0.62)
                    ctx.lineTo(x(0.24), h * 0.76)
                    ctx.quadraticCurveTo(x(0.24), h * 0.9, x(0.36), h * 0.9)
                    ctx.stroke()
                }
            }

            Connections {
                target: root
                function onColorChanged() {
                    braces.requestPaint()
                }
            }
        }
    }

    // A clock face at ten to three.
    Component {
        id: clockShape

        Item {
            Rectangle {
                anchors.fill: parent
                anchors.margins: root.width * 0.06
                radius: width / 2
                color: "transparent"
                border.width: root.stroke
                border.color: root.color
                antialiasing: true
            }
            Repeater {
                // The hands, from the middle: up, then to the right and down a little.
                model: [[0.3, 90, 0.5, 0.36], [0.24, 34, 0.6, 0.57]]

                Rectangle {
                    required property var modelData

                    width: root.width * modelData[0]
                    height: root.stroke
                    radius: root.stroke / 2
                    color: root.color
                    antialiasing: true
                    x: root.width * modelData[2] - width / 2
                    y: root.height * modelData[3] - height / 2
                    rotation: modelData[1]
                }
            }
        }
    }

    // A microphone: its head, the cradle around it, and the stand.
    Component {
        id: micShape

        Canvas {
            id: mic
            antialiasing: true
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                const w = width, h = height, s = root.stroke
                ctx.strokeStyle = root.color
                ctx.lineWidth = s
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                // The head, a capsule.
                const r = w * 0.16
                ctx.beginPath()
                ctx.arc(w * 0.5, h * 0.06 + r, r, Math.PI, 0)
                ctx.lineTo(w * 0.5 + r, h * 0.52 - r)
                ctx.arc(w * 0.5, h * 0.52 - r, r, 0, Math.PI)
                ctx.closePath()
                ctx.stroke()
                // The cradle, the stem and the foot.
                ctx.beginPath()
                ctx.arc(w * 0.5, h * 0.42, w * 0.3, 0, Math.PI)
                ctx.moveTo(w * 0.5, h * 0.72)
                ctx.lineTo(w * 0.5, h * 0.92)
                ctx.moveTo(w * 0.32, h * 0.92)
                ctx.lineTo(w * 0.68, h * 0.92)
                ctx.stroke()
            }

            Connections {
                target: root
                function onColorChanged() {
                    mic.requestPaint()
                }
            }
        }
    }

    // A flag: its pole, and the flag flying filled from its top.
    Component {
        id: flagShape

        Item {
            Rectangle {
                x: root.width * 0.16
                width: root.stroke
                height: root.height
                radius: root.stroke / 2
                color: root.color
            }
            Rectangle {
                x: root.width * 0.16 + root.stroke
                y: root.height * 0.04
                width: root.width * 0.66
                height: root.height * 0.5
                radius: root.stroke / 2
                color: root.color
            }
        }
    }

    // A filled square, to stop.
    Component {
        id: stopShape

        Item {
            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 0.64
                height: width
                radius: root.stroke
                color: root.color
            }
        }
    }

    // A player's controls: play, pause, and the sound on or off.
    Component {
        id: mediaShape

        Canvas {
            id: media

            antialiasing: true
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                // On a 24-unit grid inset by half the stroke, as the chevrons are.
                const size = Math.min(width, height)
                const unit = (size - root.stroke) / 24
                const atX = v => (width - size) / 2 + root.stroke / 2 + v * unit
                const atY = v => (height - size) / 2 + root.stroke / 2 + v * unit
                ctx.strokeStyle = root.color
                ctx.fillStyle = root.color
                ctx.lineWidth = root.stroke
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                const shape = points => {
                    ctx.beginPath()
                    ctx.moveTo(atX(points[0][0]), atY(points[0][1]))
                    for (const point of points.slice(1))
                        ctx.lineTo(atX(point[0]), atY(point[1]))
                    ctx.closePath()
                    ctx.fill()
                    ctx.stroke()
                }
                const line = (from, to) => {
                    ctx.beginPath()
                    ctx.moveTo(atX(from[0]), atY(from[1]))
                    ctx.lineTo(atX(to[0]), atY(to[1]))
                    ctx.stroke()
                }
                if (root.name === "play") {
                    shape([[7, 4], [20, 12], [7, 20]])
                } else if (root.name === "pause") {
                    shape([[6, 4], [10, 4], [10, 20], [6, 20]])
                    shape([[14, 4], [18, 4], [18, 20], [14, 20]])
                } else {
                    // The speaker, then its waves, or a cross for none.
                    shape([[3, 9], [7, 9], [12, 4], [12, 20], [7, 15], [3, 15]])
                    if (root.name === "muted") {
                        line([16, 9], [22, 15])
                        line([22, 9], [16, 15])
                    } else {
                        ctx.beginPath()
                        ctx.arc(atX(12), atY(12), 5 * unit, -Math.PI / 4, Math.PI / 4)
                        ctx.stroke()
                        ctx.beginPath()
                        ctx.arc(atX(12), atY(12), 9 * unit, -Math.PI / 4, Math.PI / 4)
                        ctx.stroke()
                    }
                }
            }
            Connections {
                target: root
                function onNameChanged() {
                    media.requestPaint()
                }
                function onColorChanged() {
                    media.requestPaint()
                }
            }
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }
    }

    // The video camera's body.
    Component {
        id: videoShape

        Item {
            Rectangle {
                x: 0
                y: root.height * 0.24
                width: root.width * 0.62
                height: root.height * 0.52
                radius: root.stroke * 1.2
                color: "transparent"
                border.width: root.stroke * 0.85
                border.color: root.color
                antialiasing: true
            }
        }
    }

    // Maximize is a rounded square; restore adds a second one behind it.
    Component {
        id: frameShape

        Item {
            Rectangle {
                anchors.centerIn: parent
                width: root.width * (root.name === "restore" ? 0.48 : 0.6)
                height: width
                radius: root.stroke
                color: "transparent"
                border.width: root.stroke * 0.85
                border.color: root.color
                antialiasing: true
            }
            Rectangle {
                visible: root.name === "restore"
                x: root.width * 0.38
                y: root.height * 0.14
                width: root.width * 0.48
                height: width
                radius: root.stroke
                color: "transparent"
                border.width: root.stroke * 0.85
                border.color: root.color
                opacity: 0.6
                antialiasing: true
            }
        }
    }
}
