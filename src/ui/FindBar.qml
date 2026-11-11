pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// Finds text in the note shown, and replaces it: what it finds is painted in the note, the
/// match it is on more strongly, and Return and Shift+Return go to the next and previous.
Rectangle {
    id: root

    /// The note's styler, which finds, paints and replaces.
    required property var styler
    /// The note's text field, to select the match in and to go back to.
    required property var body
    /// Whether the note can be changed, and so replaced in.
    property bool writable: true
    /// Whether the replace row shows.
    property bool replacing: false
    property bool matchCase: false
    property bool wholeWords: false
    /// The starts of the matches, in order, and the index of the one selected.
    property var starts: []
    property int current: -1
    readonly property int length: findField.text.length

    /// Shows the bar, with the replace row when `replace`, and the selection to find when there
    /// is one on a single line.
    function show(replace) {
        replacing = replace && writable
        visible = true
        const selected = body.selectedText
        if (selected !== "" && selected.indexOf("\n") < 0 && selected.indexOf(" ") < 0)
            findField.text = selected
        findField.forceActiveFocus()
        findField.selectAll()
        refresh(true)
    }

    function close() {
        visible = false
        starts = []
        current = -1
        styler.setFound([], 0, -1)
        body.forceActiveFocus()
    }

    /// Finds again, as after the text or an option changed; `fromCursor` selects the first match
    /// at or after the cursor.
    function refresh(fromCursor) {
        if (!visible)
            return
        starts = findField.text === "" ? [] : styler.findAll(findField.text, matchCase, wholeWords)
        if (starts.length === 0) {
            current = -1
        } else if (fromCursor || current < 0 || current >= starts.length) {
            const at = body.selectionStart
            current = starts.findIndex(start => start >= at)
            if (current < 0)
                current = 0
        }
        styler.setFound(starts, length, current)
        if (fromCursor)
            select()
    }

    /// Moves to the next match, or the one before when `back`, round the note's ends.
    function step(back) {
        if (starts.length === 0)
            return
        current = (current + (back ? -1 : 1) + starts.length) % starts.length
        styler.setFound(starts, length, current)
        select()
    }

    function select() {
        if (current < 0)
            return
        body.select(starts[current], starts[current] + length)
    }

    function replaceOne() {
        if (current < 0 || !writable)
            return
        const at = starts[current]
        styler.replace(at, at + length, replaceField.text)
        body.cursorPosition = at + replaceField.text.length
        refresh(true)
    }

    function replaceEvery() {
        if (starts.length === 0 || !writable)
            return
        styler.replaceAll(starts, length, replaceField.text)
        refresh(true)
    }

    objectName: "findBar"
    visible: false
    width: Math.min(parent ? parent.width - 2 * Theme.space4 : 480, 480)
    implicitHeight: rows.implicitHeight + 2 * Theme.space3
    height: implicitHeight
    radius: Theme.radiusLg
    color: Theme.surface
    border.color: Theme.border

    SoftShadow {
        anchors.fill: parent
        radius: parent.radius
    }

    // A note edited under the bar is searched again, staying on the match it was on.
    Connections {
        target: root.body
        function onTextChanged() {
            Qt.callLater(root.refresh, false)
        }
    }

    Column {
        id: rows

        anchors {
            fill: parent
            margins: Theme.space3
        }
        spacing: Theme.space2

        Row {
            width: parent.width
            spacing: Theme.space1

            Field {
                id: findField

                objectName: "findField"
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - count.width - caseButton.width - wordsButton.width - previous.width
                       - next.width - closeButton.width - 6 * parent.spacing
                placeholderText: qsTr("Find in the note")
                Accessible.name: qsTr("Find")
                onTextChanged: root.refresh(true)
                Keys.onReturnPressed: event => root.step(event.modifiers & Qt.ShiftModifier)
                Keys.onEnterPressed: event => root.step(event.modifiers & Qt.ShiftModifier)
                Keys.onEscapePressed: root.close()
            }
            Text {
                id: count

                objectName: "findCount"
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(implicitWidth, 56)
                horizontalAlignment: Text.AlignHCenter
                text: findField.text === "" ? "" : root.starts.length === 0 ? qsTr("None") : qsTr(
                                                                                  "%1 of %2").arg(
                                                                                  root.current
                                                                                  + 1).arg(root.starts.length)
                color: root.starts.length === 0 && findField.text !== "" ? Theme.danger :
                                                                           Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSm
            }
            PillButton {
                id: caseButton

                objectName: "findMatchCase"
                anchors.verticalCenter: parent.verticalCenter
                label: "Aa"
                selected: root.matchCase
                focusPolicy: Qt.NoFocus
                Accessible.name: qsTr("Match case")
                tip: Accessible.name
                onClicked: {
                    root.matchCase = !root.matchCase
                    root.refresh(true)
                }
            }
            PillButton {
                id: wordsButton

                objectName: "findWholeWords"
                anchors.verticalCenter: parent.verticalCenter
                label: "W"
                selected: root.wholeWords
                focusPolicy: Qt.NoFocus
                Accessible.name: qsTr("Whole words")
                tip: Accessible.name
                onClicked: {
                    root.wholeWords = !root.wholeWords
                    root.refresh(true)
                }
            }
            StickerButton {
                id: previous

                anchors.verticalCenter: parent.verticalCenter
                glyph: "chevron-up"
                enabled: root.starts.length > 0
                focusPolicy: Qt.NoFocus
                Accessible.name: qsTr("Previous match (Shift+Return)")
                tip: Accessible.name
                onClicked: root.step(true)
            }
            StickerButton {
                id: next

                anchors.verticalCenter: parent.verticalCenter
                glyph: "chevron-down"
                enabled: root.starts.length > 0
                focusPolicy: Qt.NoFocus
                Accessible.name: qsTr("Next match (Return)")
                tip: Accessible.name
                onClicked: root.step(false)
            }
            CloseButton {
                id: closeButton

                anchors.verticalCenter: parent.verticalCenter
                onClicked: root.close()
            }
        }

        Row {
            visible: root.replacing
            width: parent.width
            spacing: Theme.space1

            Field {
                id: replaceField

                objectName: "replaceField"
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - replaceButton.width - replaceAllButton.width - 2
                       * parent.spacing
                placeholderText: qsTr("Replace with")
                Accessible.name: qsTr("Replace with")
                Keys.onReturnPressed: root.replaceOne()
                Keys.onEnterPressed: root.replaceOne()
                Keys.onEscapePressed: root.close()
            }
            StickerButton {
                id: replaceButton

                objectName: "replaceOne"
                anchors.verticalCenter: parent.verticalCenter
                enabled: root.current >= 0
                focusPolicy: Qt.NoFocus
                text: qsTr("Replace")
                onClicked: root.replaceOne()
            }
            StickerButton {
                id: replaceAllButton

                objectName: "replaceAll"
                anchors.verticalCenter: parent.verticalCenter
                enabled: root.starts.length > 0
                focusPolicy: Qt.NoFocus
                text: qsTr("Replace all")
                onClicked: root.replaceEvery()
            }
        }
    }
}
