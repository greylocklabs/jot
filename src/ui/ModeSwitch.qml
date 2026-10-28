pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// A choice between a few modes, as a slider: the accent slides under the one chosen.
Rectangle {
    id: root

    /// The choices: {label, tip} for each.
    required property var options
    /// Which is chosen, by index.
    property int current: 0

    signal picked(int index)

    readonly property real inset: Theme.space1

    implicitWidth: choiceRow.implicitWidth + 2 * inset
    implicitHeight: Theme.switchHeight
    radius: height / 2
    color: Theme.surfaceAlt

    Rectangle {
        id: thumb

        objectName: "thumb"
        // Under the chosen choice, each as wide as its own label.
        readonly property Item chosen: choices.count > root.current ? choices.itemAt(root.current) :
                                                                      null

        x: root.inset + (chosen ? chosen.x : 0)
        y: root.inset
        width: chosen ? chosen.width : 0
        height: root.height - 2 * root.inset
        radius: height / 2
        color: Theme.accent

        Behavior on x {
            NumberAnimation {
                duration: Theme.durMed
                easing.type: Theme.easingBounce
                easing.overshoot: Theme.bounce
            }
        }
        Behavior on width {
            NumberAnimation {
                duration: Theme.durMed
                easing.type: Theme.easingBounce
                easing.overshoot: Theme.bounce
            }
        }
    }

    Row {
        id: choiceRow

        x: root.inset
        y: root.inset
        height: root.height - 2 * root.inset

        Repeater {
            id: choices
            model: root.options.length

            Choice {}
        }
    }

    component Choice: AbstractButton {
        id: choice

        required property int index
        readonly property bool chosen: root.current === index
        readonly property var option: root.options[index]

        objectName: "choice" + index
        width: implicitWidth
        height: parent.height
        // Measured in the heaviest weight, so choosing a side does not change the width.
        implicitWidth: widest.advanceWidth + 2 * Theme.space3
        text: option.label
        focusPolicy: Qt.NoFocus
        Accessible.role: Accessible.RadioButton
        Accessible.checked: chosen
        onClicked: root.picked(index)

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }
        TextMetrics {
            id: widest
            text: choice.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
            font.weight: Theme.weightHeavy
        }

        contentItem: Text {
            id: caption
            text: choice.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: choice.chosen ? Theme.accentText : choice.hovered ? Theme.text : Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
            font.weight: choice.chosen ? Theme.weightHeavy : Theme.weightBold

            Behavior on color {
                ColorAnimation {
                    duration: Theme.durFast
                }
            }
        }
        background: null

        // What the choice does, and its shortcut, after resting on it a moment.
        Loader {
            active: choice.option.tip !== "" && choice.hovered

            sourceComponent: Tip {
                parent: choice
                visible: !choice.pressed
                text: choice.option.tip
                delay: Theme.durSlow * 2
                // An open popup that takes Escape keeps the window's shortcuts from working.
                closePolicy: Popup.NoAutoClose
            }
        }
    }
}
