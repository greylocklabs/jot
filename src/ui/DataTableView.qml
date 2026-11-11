pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// A data file's records as a table: one row per record and one column per field. A click picks
/// a cell, typing or Enter edits it, and Delete empties it; the headers sort, rename and remove.
Item {
    id: root

    // Shift and the wheel scroll across the columns, as in a spreadsheet, from the headers too.
    component AcrossWheel: WheelHandler {
        acceptedModifiers: Qt.ShiftModifier
        onWheel: event => {
            const delta = event.angleDelta.y !== 0 ? event.angleDelta.y : event.angleDelta.x
            const most = Math.max(0, table.contentWidth - table.width)
            table.contentX = Math.max(0, Math.min(most, table.contentX - delta))
        }
    }

    required property DataTableModel records
    /// The cell picked, by the row and column shown.
    property int row: 0
    property int column: 0
    readonly property bool editing: cellEditor.visible

    // Undo and redo the table's edits from the keyboard. The fields typed in take the keys for
    // their own typing first; here they would otherwise go to the window's to-do undo.
    // Written out as well, since not every platform theme gives the standard keys these.
    function isUndo(event) {
        return event.matches(StandardKey.Undo) || (event.key === Qt.Key_Z && event.modifiers
                                                   === Qt.ControlModifier)
    }
    function isRedo(event) {
        return event.matches(StandardKey.Redo) || (event.key === Qt.Key_Z && event.modifiers === (
                                                       Qt.ControlModifier | Qt.ShiftModifier)) || (
                    event.key === Qt.Key_Y && event.modifiers === Qt.ControlModifier)
    }
    Keys.onShortcutOverride: event => {
        if (root.isUndo(event) || root.isRedo(event))
            event.accepted = true
    }
    Keys.onPressed: event => {
        if (root.isUndo(event)) {
            root.records.undo()
            event.accepted = true
        } else if (root.isRedo(event)) {
            root.records.redo()
            event.accepted = true
        }
    }

    /// Starts editing the picked cell, with `typed` in place of its value when given.
    function edit(typed) {
        if (root.records.columns.length === 0 || root.records.rowCount() === 0)
            return
        const cell = root.records.index(root.row, root.column)
        if (root.records.data(cell, DataTableModel.TypeRole) === "nested" && typed === undefined) {
            nestedEditor.openFor(root.row, root.column)
            return
        }
        // Open at once, so the next key typed lands in it; placed once the cell is in view.
        cellEditor.original = root.records.data(cell)
        cellEditor.text = typed !== undefined ? typed : cellEditor.original
        cellEditor.visible = true
        cellEditor.forceActiveFocus()
        if (typed === undefined)
            cellEditor.selectAll()
        table.positionViewAtCell(Qt.point(root.column, root.row), TableView.Contain)
        root.placeEditor()
        Qt.callLater(root.placeEditor)
    }

    function placeEditor() {
        const item = table.itemAtCell(Qt.point(root.column, root.row))
        if (!item)
            return
        const at = item.mapToItem(root, 0, 0)
        cellEditor.x = at.x
        cellEditor.y = at.y
        cellEditor.width = item.width
        cellEditor.height = item.height
    }

    function commit() {
        if (!cellEditor.visible)
            return
        cellEditor.visible = false
        // Left as it was, it is not written, so its type and quoting stay too.
        if (cellEditor.text === cellEditor.original) {
            table.forceActiveFocus()
            return
        }
        if (!root.records.setCell(root.row, root.column, cellEditor.text))
            problem.text = root.records.lastError()
        else
            problem.text = ""
        table.forceActiveFocus()
    }

    function pick(row, column) {
        root.row = Math.max(0, Math.min(row, root.records.rowCount() - 1))
        root.column = Math.max(0, Math.min(column, root.records.columns.length - 1))
        table.positionViewAtCell(Qt.point(root.column, root.row), TableView.Contain)
    }

    // A file of several lists shows one at a time: a tab for each, by its key or document.
    Flow {
        id: partTabs

        objectName: "partTabs"
        visible: root.records.parts.length > 0
        width: parent.width
        height: visible ? implicitHeight : 0
        spacing: Theme.space1

        Repeater {
            model: root.records.parts

            PillButton {
                required property string modelData
                required property int index

                label: modelData
                selected: root.records.part === index
                onClicked: root.records.part = index
            }
        }
    }

    // The tools wrap onto more lines on a narrow note, rather than running off its edge.
    Flow {
        id: tools

        objectName: "tableTools"
        anchors.top: partTabs.bottom
        anchors.topMargin: partTabs.visible ? Theme.space2 : 0
        width: parent.width
        spacing: Theme.space2

        Field {
            id: filterField

            objectName: "tableFilter"
            width: Math.min(220, tools.width)
            placeholderText: qsTr("Filter rows")
            text: root.records.filter
            onTextChanged: root.records.filter = text
        }
        StickerButton {
            objectName: "addRow"
            text: qsTr("Add row")
            enabled: Vault.writable
            onClicked: {
                const shown = root.records.addRowAfter(root.records.rowCount() === 0 ? -1 :
                                                                                       root.row)
                if (shown < 0)
                    return
                root.pick(shown, 0)
                root.edit()
            }
        }
        StickerButton {
            objectName: "addColumn"
            text: qsTr("Add column")
            enabled: Vault.writable
            onClicked: {
                newColumn.text = ""
                newColumn.visible = true
                newColumn.forceActiveFocus()
            }
        }
        Field {
            id: newColumn

            objectName: "newColumn"
            visible: false
            width: Math.min(180, tools.width)
            placeholderText: qsTr("Column name")
            onAccepted: {
                if (root.records.addColumn(text)) {
                    visible = false
                    problem.text = ""
                    root.pick(root.row, root.records.columns.length - 1)
                } else {
                    problem.text = root.records.lastError()
                }
            }
            Keys.onEscapePressed: visible = false
        }
        StickerButton {
            text: qsTr("Undo")
            enabled: root.records.canUndo
            onClicked: root.records.undo()
        }
        StickerButton {
            text: qsTr("Redo")
            enabled: root.records.canRedo
            onClicked: root.records.redo()
        }
    }

    Text {
        id: problem

        objectName: "tableProblem"
        anchors.top: tools.bottom
        anchors.topMargin: text === "" ? 0 : Theme.space2
        width: parent.width
        height: text === "" ? 0 : implicitHeight
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
        color: Theme.danger
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textSm
        font.weight: Theme.weightStrong
    }

    // What breaks the schema: the picked cell's reason, else how many values do.
    Text {
        id: schemaLine

        readonly property string here: root.records.columns.length > 0 && root.records.rowCount()
                                       > 0 ? root.records.data(root.records.index(root.row,
                                                                                  root.column),
                                                               DataTableModel.ProblemRole) || "" :
                                             ""

        objectName: "schemaLine"
        anchors.top: problem.bottom
        anchors.topMargin: Theme.space2
        width: parent.width
        visible: text !== ""
        text: here !== "" ? here : root.records.schemaSummary
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
        color: Theme.danger
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textSm
    }

    HorizontalHeaderView {
        id: header

        objectName: "tableHeader"
        anchors.top: schemaLine.visible ? schemaLine.bottom : problem.bottom
        anchors.topMargin: Theme.space3
        anchors.left: table.left
        anchors.right: parent.right
        syncView: table
        clip: true
        interactive: false

        AcrossWheel {}

        delegate: Rectangle {
            id: heading

            required property int index
            required property string display
            property bool renaming: false

            readonly property bool sorted: root.records.sortColumn === index

            implicitWidth: 180
            implicitHeight: 32
            // Lit on hover, as a button is, since a click sorts by it.
            color: headingHover.hovered && !renaming ? Theme.border : Theme.surfaceAlt
            border.color: Theme.border
            Accessible.role: Accessible.ColumnHeader
            Accessible.name: display

            Text {
                anchors.fill: parent
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space6 + Theme.space4
                visible: !heading.renaming
                verticalAlignment: Text.AlignVCenter
                text: heading.display
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSm
                font.weight: Theme.weightStrong
            }
            TextInput {
                id: rename

                anchors.fill: parent
                anchors.leftMargin: Theme.space3
                visible: heading.renaming
                verticalAlignment: Text.AlignVCenter
                color: Theme.text
                selectionColor: Theme.tint(Theme.accent, 0.4)
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSm
                onAccepted: {
                    heading.renaming = false
                    if (!root.records.renameColumn(heading.index, text))
                        problem.text = root.records.lastError()
                }
                onActiveFocusChanged: if (!activeFocus)
                                          heading.renaming = false
                Keys.onEscapePressed: heading.renaming = false
            }
            HoverHandler {
                id: headingHover
                cursorShape: heading.renaming ? Qt.IBeamCursor : Qt.PointingHandCursor
            }
            // Which way it is sorted, or on hover that a click sorts by it.
            Glyph {
                objectName: "sortArrow"
                anchors {
                    right: removeColumn.visible ? removeColumn.left : parent.right
                    rightMargin: removeColumn.visible ? 0 : Theme.space3
                    verticalCenter: parent.verticalCenter
                }
                width: Theme.smallGlyphSize
                height: width
                visible: !heading.renaming && (heading.sorted || headingHover.hovered)
                name: "chevron-down"
                rotation: heading.sorted && !root.records.sortDescending ? 180 : 0
                color: heading.sorted ? Theme.accent : Theme.textFaint
                stroke: Theme.smallGlyphStroke
            }
            Tip {
                visible: headingHover.hovered && !heading.renaming && !removeColumn.hovered
                delay: Theme.durSlow * 2
                text: !heading.sorted ? qsTr("Sort by %1").arg(heading.display) :
                                        root.records.sortDescending ? qsTr(
                                                                          "Sorted Z to A, click for the file's order") :
                                                                      qsTr("Sorted A to Z, click for Z to A")
            }
            // Clicks sort and double clicks rename only beside the X, so on any Qt version the X's
            // own clicks reach nothing else.
            Item {
                anchors {
                    left: parent.left
                    top: parent.top
                    bottom: parent.bottom
                    right: removeColumn.visible ? removeColumn.left : parent.right
                }

                // A click sorts once it is clear no second click makes it a double click to
                // rename, so renaming never sorts the table under the field.
                Timer {
                    id: sortSoon

                    interval: Application.styleHints.mouseDoubleClickInterval
                    onTriggered: root.records.sortBy(heading.index)
                }
                TapHandler {
                    onTapped: {
                        if (!Vault.writable)
                            root.records.sortBy(heading.index)
                        else if (tapCount === 1)
                            sortSoon.restart()
                    }
                    onDoubleTapped: {
                        if (!Vault.writable)
                            return
                        sortSoon.stop()
                        rename.text = heading.display
                        heading.renaming = true
                        rename.forceActiveFocus()
                        rename.selectAll()
                    }
                }
            }
            RemoveButton {
                id: removeColumn

                objectName: "removeColumn"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: Vault.writable && headingHover.hovered && !heading.renaming
                scale: 0.75
                Accessible.name: armed ? qsTr("Click again to remove the column %1").arg(
                                             heading.display) : qsTr("Remove the column %1").arg(
                                             heading.display)
                onConfirmed: root.records.removeColumn(heading.index)
            }
        }
    }

    VerticalHeaderView {
        id: rowHeader

        objectName: "rowHeader"
        anchors.top: table.top
        anchors.bottom: table.bottom
        anchors.left: parent.left
        syncView: table
        clip: true
        interactive: false

        AcrossWheel {}

        delegate: Rectangle {
            id: number

            required property int index
            required property var display

            implicitWidth: 48
            implicitHeight: 32
            color: Theme.surfaceAlt
            border.color: Theme.border

            Text {
                anchors.centerIn: parent
                visible: !numberHover.hovered || !Vault.writable
                text: number.display
                color: Theme.textFaint
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textXs
            }
            HoverHandler {
                id: numberHover
            }
            RemoveButton {
                objectName: "removeRow"
                anchors.centerIn: parent
                visible: Vault.writable && numberHover.hovered
                scale: 0.75
                Accessible.name: armed ? qsTr("Click again to remove row %1").arg(number.display) :
                                         qsTr("Remove row %1").arg(number.display)
                onConfirmed: {
                    root.records.removeRow(number.index)
                    // The button goes with its row, so the keys come back to the table.
                    table.forceActiveFocus()
                }
            }
        }
    }

    TableView {
        id: table

        objectName: "table"
        anchors.top: header.bottom
        anchors.left: rowHeader.right
        anchors.right: parent.right
        // Only as tall as its rows, so the bar for scrolling across sits right under the last.
        height: Math.min(contentHeight + (across.visible ? across.height : 0), parent.height - y)
        clip: true
        model: root.records
        boundsBehavior: Flickable.StopAtBounds
        columnWidthProvider: () => 180
        focus: true
        // Shown whenever there is more to see, not only while scrolling.
        ScrollBar.horizontal: ScrollBar {
            id: across

            policy: table.contentWidth > table.width ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
        }
        ScrollBar.vertical: ScrollBar {
            policy: table.contentHeight > table.height ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
        }

        AcrossWheel {}

        Keys.onPressed: event => {
            if (root.editing)
                return
            switch (event.key) {
            case Qt.Key_Up:
                root.pick(root.row - 1, root.column)
                break
            case Qt.Key_Down:
                root.pick(root.row + 1, root.column)
                break
            case Qt.Key_Left:
                root.pick(root.row, root.column - 1)
                break
            case Qt.Key_Right:
            case Qt.Key_Tab:
                root.pick(root.row, root.column + 1)
                break
            case Qt.Key_Backtab:
                root.pick(root.row, root.column - 1)
                break
            case Qt.Key_Return:
            case Qt.Key_Enter:
            case Qt.Key_F2:
                if (Vault.writable)
                    root.edit()
                break
            case Qt.Key_Delete:
            case Qt.Key_Backspace:
                if (Vault.writable)
                    root.records.clearCell(root.row, root.column)
                break
            default:
                if (Vault.writable && event.text.length > 0 && event.text >= " " && !(event.modifiers
                                                                                      & (Qt.ControlModifier
                                                                                         | Qt.AltModifier))) {
                    root.edit(event.text)
                    break
                }
                return
            }
            event.accepted = true
        }

        delegate: Rectangle {
            id: cell

            required property int row
            required property int column
            required property string display
            required property string type
            required property bool stray
            required property string problem
            readonly property bool picked: root.row === row && root.column === column

            implicitHeight: 32
            // Through cell: the line above the table is named problem too, and an id wins.
            color: cell.problem !== "" ? Theme.tint(Theme.danger, 0.3) : cell.stray ? Theme.tint(
                                                                                          Theme.danger,
                                                                                          0.18) : Theme.bg
            border.width: picked ? 2 : 1
            border.color: picked ? Theme.accent : Theme.border

            Text {
                anchors.fill: parent
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space3
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: cell.type === "number" ? Text.AlignRight : Text.AlignLeft
                text: cell.display.replace(/\n/g, " ")
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: cell.type === "null" || cell.type === "nested" ? Theme.textFaint : Theme.text
                font.family: cell.type === "nested" ? Theme.monoFontFamily : Theme.fontFamily
                font.italic: cell.type === "null"
                font.pixelSize: Theme.textSm
            }
            TapHandler {
                onTapped: {
                    root.commit()
                    root.row = cell.row
                    root.column = cell.column
                    table.forceActiveFocus()
                }
                onDoubleTapped: {
                    root.row = cell.row
                    root.column = cell.column
                    if (Vault.writable)
                        root.edit()
                }
            }
        }
    }

    // Typing in a cell, laid over it.
    TextField {
        id: cellEditor

        /// The value as it was when editing started.
        property string original

        objectName: "cellEditor"
        visible: false
        z: 2
        padding: Theme.space2
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textSm
        background: Rectangle {
            color: Theme.surface
            border.width: 2
            border.color: Theme.accent
        }
        onAccepted: {
            root.commit()
            root.pick(root.row + 1, root.column)
        }
        onActiveFocusChanged: if (!activeFocus)
                                  root.commit()
        Keys.onEscapePressed: {
            visible = false
            table.forceActiveFocus()
        }
        Keys.onTabPressed: {
            root.commit()
            root.pick(root.row, root.column + 1)
        }
    }

    // The values the schema allows, under the cell being typed in; a click takes one.
    Rectangle {
        id: choiceList

        readonly property var choices: cellEditor.visible ? root.records.choices(root.column) : []
        readonly property var shown: choices.filter(c => c.toLowerCase().includes(
                                                             cellEditor.text.toLowerCase())
                                                         || cellEditor.text === cellEditor.original)

        objectName: "choices"
        visible: cellEditor.visible && shown.length > 0
        z: 3
        x: cellEditor.x
        y: cellEditor.y + cellEditor.height
        width: Math.max(cellEditor.width, 160)
        height: Math.min(choiceColumn.implicitHeight, 200)
        color: Theme.surface
        border.color: Theme.border
        radius: Theme.radiusSm
        clip: true

        Column {
            id: choiceColumn

            width: parent.width

            Repeater {
                model: choiceList.shown

                ItemDelegate {
                    required property string modelData

                    width: choiceColumn.width
                    text: modelData
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textSm
                    // Taken before the editor loses focus to the click.
                    focusPolicy: Qt.NoFocus
                    onClicked: {
                        cellEditor.text = modelData
                        root.commit()
                    }
                }
            }
        }
    }

    // A nested object or list, edited as JSON.
    Popup {
        id: nestedEditor

        property int row
        property int column

        /// The value as shown when opened, so Save with nothing changed writes nothing.
        property string original

        function openFor(row, column) {
            nestedEditor.row = row
            nestedEditor.column = column
            nestedEditor.original = root.records.nestedText(row, column)
            nestedText.text = nestedEditor.original
            nestedProblem.text = ""
            open()
            nestedText.forceActiveFocus()
        }

        objectName: "nestedEditor"
        anchors.centerIn: Overlay.overlay
        width: Math.min(560, root.width)
        height: Math.min(420, root.height)
        modal: true
        padding: Theme.space5
        background: Rectangle {
            radius: Theme.radiusLg
            color: Theme.surface
            border.color: Theme.border
        }

        Column {
            anchors.fill: parent
            spacing: Theme.space3

            ScrollView {
                width: parent.width
                height: parent.height - nestedProblem.height - buttons.height - 2 * parent.spacing

                TextArea {
                    id: nestedText

                    objectName: "nestedText"
                    readOnly: !Vault.writable
                    color: Theme.text
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.textSm
                    wrapMode: TextArea.NoWrap
                }
            }
            Text {
                id: nestedProblem

                width: parent.width
                height: text === "" ? 0 : implicitHeight
                wrapMode: Text.WordWrap
                color: Theme.danger
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSm
            }
            Row {
                id: buttons

                anchors.right: parent.right
                spacing: Theme.space2

                StickerButton {
                    text: qsTr("Cancel")
                    onClicked: nestedEditor.close()
                }
                StickerButton {
                    objectName: "saveNested"
                    accent: true
                    enabled: Vault.writable
                    text: qsTr("Save")
                    onClicked: {
                        if (nestedText.text === nestedEditor.original)
                            nestedEditor.close()
                        else if (root.records.setCellAs(nestedEditor.row, nestedEditor.column,
                                                        nestedText.text, "nested"))
                            nestedEditor.close()
                        else
                            nestedProblem.text = root.records.lastError()
                    }
                }
            }
        }
    }
}
