pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// Everything with a tag, its notes and its open to-dos together, in place of the note list.
Rectangle {
    id: root

    /// A note or a to-do chosen, which opens it.
    signal picked
    /// Esc in the view, to go out to the sidebar.
    signal leftList

    /// The notes' rows, then the to-dos', top to bottom.
    function rows() {
        const found = []
        for (const child of column.children) {
            if (child.isTagRow === true)
                found.push(child)
        }
        return found
    }

    /// Puts the keyboard on the first row.
    function focusList() {
        const all = rows()
        if (all.length > 0 && !all.some(row => row.activeFocus))
            all[0].forceActiveFocus()
    }

    /// Moves to the next, previous, first or last row and opens it, by KeyRouter's action id.
    function step(id) {
        const all = rows()
        if (all.length === 0)
            return
        let at = all.findIndex(row => row.activeFocus)
        if (id === "firstNote" || at < 0)
            at = 0
        else if (id === "lastNote")
            at = all.length - 1
        else if (id === "nextNote")
            at = Math.min(at + 1, all.length - 1)
        else
            at = Math.max(at - 1, 0)
        all[at].open()
        // The note shows it, while the keys stay here to move on.
        all[at].forceActiveFocus()
    }

    color: Theme.bg

    component Heading: Text {
        width: parent.width
        leftPadding: Theme.space5
        topPadding: Theme.space4
        bottomPadding: Theme.space1
        elide: Text.ElideRight
        color: Theme.textFaint
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textXs
        font.weight: Theme.weightBold
        font.capitalization: Font.AllUppercase
    }

    component Row: AbstractButton {
        id: row

        property string label
        property string detail
        readonly property bool isTagRow: true

        /// Opens the note or the to-do, as a click does.
        function open() {
            clicked()
        }

        width: parent.width
        height: Theme.listRowHeight + (detail !== "" ? Theme.textSm : 0)
        hoverEnabled: true
        Accessible.name: label
        Keys.onEscapePressed: root.leftList()
        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()

        background: Rectangle {
            anchors.fill: parent
            anchors.leftMargin: Theme.space3
            anchors.rightMargin: Theme.space3
            radius: Theme.radiusSm
            color: row.activeFocus ? Theme.tint(Theme.accent, 0.18) : row.hovered ? Theme.surfaceAlt :
                                                                                    Theme.bg
        }
        contentItem: Column {
            leftPadding: Theme.space5
            rightPadding: Theme.space5
            topPadding: Theme.space1

            Text {
                width: parent.width - parent.leftPadding - parent.rightPadding
                text: row.label
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textMd
            }
            Text {
                visible: row.detail !== ""
                width: parent.width - parent.leftPadding - parent.rightPadding
                text: row.detail
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: Theme.textFaint
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSm
            }
        }
    }

    // The tag, with the way back to where you were.
    Item {
        id: header

        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: Theme.space4
        }
        height: chip.height

        PillButton {
            id: chip

            objectName: "tagChip"
            label: "#" + Vault.shownTag + "  ×"
            selected: true
            tip: qsTr("Back to where you were")
            Accessible.name: qsTr("Close #%1").arg(Vault.shownTag)
            onClicked: Vault.shownTag = ""
        }
    }

    ScrollView {
        anchors {
            left: parent.left
            right: parent.right
            top: header.bottom
            bottom: parent.bottom
            topMargin: Theme.space2
        }
        contentWidth: availableWidth

        Column {
            id: column

            width: parent.width

            Heading {
                objectName: "tagNotesHeading"
                text: qsTr("Notes (%1)").arg(Vault.tagNotes.count)
            }
            Repeater {
                objectName: "tagNotes"
                model: Vault.tagNotes

                Row {
                    required property string path
                    required property string title
                    required property string notebook

                    objectName: "tagNote"
                    label: title
                    detail: notebook
                    onClicked: {
                        Vault.current = path
                        root.picked()
                    }
                }
            }

            Heading {
                objectName: "tagTasksHeading"
                text: qsTr("To-dos (%1)").arg(Vault.tagTasks.count)
            }
            Repeater {
                objectName: "tagTasks"
                model: Vault.tagTasks

                Row {
                    required property string path
                    required property string title
                    required property string note
                    required property int line

                    objectName: "tagTask"
                    label: title
                    detail: note
                    onClicked: {
                        Vault.openAt(path, line)
                        root.picked()
                    }
                }
            }
        }
    }
}
