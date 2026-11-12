pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// The middle column when Tasks is chosen: the to-dos due today, coming up, with no day, or
/// all of them, narrowed to a label or a tag. Ticking one changes its note; choosing one opens
/// it there.
Rectangle {
    id: root

    /// The + was pressed, to add a to-do.
    signal addRequested
    /// A to-do was chosen with a click, opening its note.
    signal taskPicked
    /// Esc in the list, to go out to the sidebar.
    signal leftList

    /// Puts the keyboard in the list of to-dos, on the first if none is chosen.
    function focusList() {
        list.forceActiveFocus()
        if (list.currentIndex < 0 && list.count > 0)
            list.currentIndex = 0
    }

    /// Moves to the next, previous, first or last to-do and opens its note there, by
    /// KeyRouter's action id.
    function step(id) {
        if (list.count === 0)
            return
        list.forceActiveFocus()
        if (id === "firstNote" || list.currentIndex < 0)
            list.currentIndex = 0
        else if (id === "lastNote")
            list.currentIndex = list.count - 1
        else if (id === "nextNote")
            list.incrementCurrentIndex()
        else
            list.decrementCurrentIndex()
    }

    color: Theme.bg

    Column {
        id: header

        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: Theme.space5
        }
        spacing: Theme.space3

        // The views as a switch with full labels, with short ones when the column is narrow,
        // or as a menu when even those do not fit.
        Item {
            id: viewsRow

            readonly property var views: ["today", "upcoming", "anytime", "all", "done"]
            readonly property var options: [
                {
                    "label": qsTr("Today"),
                    "short": qsTr("Today"),
                    "tip": qsTr("Overdue, and due today")
                },
                {
                    "label": qsTr("Upcoming"),
                    "short": qsTr("Soon"),
                    "tip": qsTr("Due after today, soonest first")
                },
                {
                    "label": qsTr("Anytime"),
                    "short": qsTr("Any"),
                    "tip": qsTr("Not due on any day")
                },
                {
                    "label": qsTr("All"),
                    "short": qsTr("All"),
                    "tip": qsTr("Everything left to do")
                },
                {
                    "label": qsTr("Done"),
                    "short": qsTr("Done"),
                    "tip": qsTr("Ticked off, most recently first")
                }
            ]
            readonly property int current: Math.max(0, views.indexOf(Vault.tasks.view))
            readonly property real room: width - (addTask.visible ? addTask.width + Theme.space3 :
                                                                    0)

            width: parent.width
            height: Math.max(viewSwitch.height, addTask.height)

            ModeSwitch {
                id: viewSwitch

                objectName: "taskViews"
                anchors.verticalCenter: parent.verticalCenter
                visible: implicitWidth <= viewsRow.room
                options: viewsRow.options
                current: viewsRow.current
                onPicked: index => Vault.tasks.view = viewsRow.views[index]
            }
            ModeSwitch {
                id: shortSwitch

                objectName: "taskViewsShort"
                anchors.verticalCenter: parent.verticalCenter
                visible: !viewSwitch.visible && implicitWidth <= viewsRow.room
                options: viewsRow.options.map(option => ({
                    "label": option.short,
                    "tip": option.label + ": " + option.tip
                }))
                current: viewsRow.current
                onPicked: index => Vault.tasks.view = viewsRow.views[index]
            }
            Dropdown {
                id: viewsMenu

                objectName: "taskViewsMenu"
                anchors.verticalCenter: parent.verticalCenter
                visible: !viewSwitch.visible && !shortSwitch.visible
                minimumWidth: 0
                maximumWidth: viewsRow.room
                model: viewsRow.views.map((view, index) => ({
                    "id": view,
                    "name": viewsRow.options[index].label
                }))
                currentValue: viewsRow.views[viewsRow.current]
                onActivated: value => Vault.tasks.view = value
            }
            StickerButton {
                id: addTask

                objectName: "addTask"
                anchors.verticalCenter: parent.verticalCenter
                x: (viewSwitch.visible ? viewSwitch.width : shortSwitch.visible ? shortSwitch.width :
                                                                                  viewsMenu.width)
                   + Theme.space3
                visible: Vault.writable
                glyph: "plus"
                Accessible.name: qsTr("Add a to-do")
                tip: qsTr("Add a to-do (Ctrl+Shift+A)")
                onClicked: root.addRequested()
            }
        }

        Row {
            width: parent.width
            spacing: Theme.space3

            Text {
                width: Math.min(implicitWidth, parent.width - priorityFilter.width - parent.spacing
                                - (taskFilter.visible ? taskFilter.width + parent.spacing : 0))
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
                text: {
                    const count = Vault.tasks.count
                    if (count === 0)
                        return qsTr("Nothing here.")
                    if (Vault.tasks.view === "done")
                        return count === 1 ? qsTr("1 done") : qsTr("%1 done").arg(count)
                    return count === 1 ? qsTr("1 thing to do") : qsTr("%1 things to do").arg(count)
                }
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textMd
                font.weight: Theme.weightStrong
            }
            // What the list is narrowed to, with a way to see it all again.
            PillButton {
                id: taskFilter

                objectName: "taskFilter"
                anchors.verticalCenter: parent.verticalCenter
                visible: Vault.tasks.tag !== ""
                label: "#" + Vault.tasks.tag
                selected: true
                tip: qsTr("Show to-dos with any tag again")
                Accessible.name: qsTr("Show to-dos with any tag again")
                onClicked: Vault.tasks.tag = ""
            }
            // Only the to-dos of one priority, or of none.
            Dropdown {
                id: priorityFilter

                objectName: "priorityFilter"
                anchors.verticalCenter: parent.verticalCenter
                minimumWidth: 0
                model: [-1, 1, 2, 3, 4, 0].map(priority => ({
                    "id": priority,
                    "name": priority < 0 ? qsTr("Any priority") : Vault.priorityName(priority)
                }))
                currentValue: Vault.tasks.priority
                Accessible.name: qsTr("Priority")
                onActivated: value => Vault.tasks.priority = value
            }
        }
    }

    ListView {
        id: list

        objectName: "taskRows"
        anchors {
            left: parent.left
            right: parent.right
            top: header.bottom
            bottom: parent.bottom
            topMargin: Theme.space3
        }
        clip: true
        model: Vault.tasks
        spacing: Theme.space1
        keyNavigationEnabled: true
        currentIndex: -1
        Keys.onEscapePressed: root.leftList()
        ScrollBar.vertical: ScrollBar {}

        section.property: "group"
        section.criteria: ViewSection.FullString
        section.delegate: Item {
            required property string section

            width: ListView.view.width
            // Ordered other than by day, the list has no headings.
            height: section !== "" ? heading.implicitHeight : 0
            visible: section !== ""

            Text {
                id: heading

                width: parent.width
                leftPadding: Theme.space5
                topPadding: Theme.space4
                bottomPadding: Theme.space1
                text: parent.section
                elide: Text.ElideRight
                color: Theme.textFaint
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textXs
                font.weight: Theme.weightBold
                font.capitalization: Font.AllUppercase
            }
        }

        delegate: AbstractButton {
            id: item

            required property string kind
            required property string path
            required property string title
            required property string note
            required property int line
            required property string due
            required property bool overdue
            required property bool repeats
            required property bool done
            required property int priority
            required property string wording

            width: ListView.view.width
            height: Math.max(Theme.listRowHeight, words.implicitHeight + 2 * Theme.space2)
            hoverEnabled: true
            Accessible.name: title
            onClicked: {
                Vault.openAt(path, line)
                root.taskPicked()
            }
            // Moved to with the keyboard, it opens its note there as a click does, and the keys
            // stay in the list to move on.
            ListView.onIsCurrentItemChanged: {
                if (!ListView.isCurrentItem || !ListView.view.activeFocus)
                    return
                Vault.openAt(path, line)
                ListView.view.forceActiveFocus()
            }
            // A double-click sets its day and repeat, as Ctrl+Shift+D does in the note.
            onDoubleClicked: {
                if (!Vault.writable || done)
                    return
                scheduler.kind = kind
                scheduler.path = path
                scheduler.line = line
                scheduler.written = wording
                scheduler.openFor(wording)
            }

            background: Rectangle {
                anchors.fill: parent
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space3
                radius: Theme.radiusSm
                color: item.ListView.isCurrentItem && list.activeFocus ? Theme.tint(Theme.accent,
                                                                                    0.18) : item.hovered
                                                                         ? Theme.surfaceAlt :
                                                                           Theme.bg
            }

            contentItem: Item {
                AbstractButton {
                    id: box

                    objectName: "taskBox"

                    x: Theme.space5
                    anchors.verticalCenter: parent.verticalCenter
                    width: Theme.textBase + Theme.space1
                    height: width
                    enabled: Vault.writable
                    Accessible.name: qsTr("Done")
                    Accessible.role: Accessible.CheckBox
                    Accessible.checked: item.done
                    // Unticked in Done, it is open again.
                    onClicked: {
                        if (item.kind === "note")
                            item.done ? Vault.reopenTask(item.path) : Vault.completeTask(item.path)
                        else
                            Vault.setTaskDone(item.path, item.line, !item.done)
                    }

                    background: Rectangle {
                        radius: Theme.radiusSm
                        color: item.done ? (box.hovered ? Theme.tint(Theme.accent, 0.6) :
                                                          Theme.accent) : box.hovered ? Theme.tint(
                                                                                            Theme.accent,
                                                                                            0.25) : Theme.surface
                        border.color: box.hovered || item.done ? Theme.accent : Theme.border
                        border.width: 2

                        Glyph {
                            anchors.centerIn: parent
                            width: parent.width - Theme.space2
                            height: width
                            visible: item.done
                            name: "check"
                            color: Theme.accentText
                        }
                    }
                }
                Column {
                    id: words

                    anchors {
                        left: box.right
                        leftMargin: Theme.space4
                        right: flag.left
                        rightMargin: Theme.space3
                        verticalCenter: parent.verticalCenter
                    }
                    spacing: Theme.space1

                    Text {
                        width: parent.width
                        text: item.title
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        color: item.done ? Theme.textMuted : Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textBase
                        font.weight: item.kind === "note" ? Theme.weightBold : Theme.weightStrong
                    }
                    // Where it is written, unless the heading says so, and whether it repeats.
                    Text {
                        width: parent.width
                        visible: text !== ""
                        text: [item.repeats ? "🔁" : "", item.note !== "" && item.note
                            !== item.ListView.section ? item.note : ""].filter(part => part !== "").join(
                            "  ")

                        elide: Text.ElideRight
                        color: Theme.textFaint
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                }
                // Its priority, flagged in the priority's color.
                Glyph {
                    id: flag

                    objectName: "priorityFlag"
                    anchors {
                        right: dueLabel.visible ? dueLabel.left : parent.right
                        rightMargin: dueLabel.visible ? Theme.space2 : Theme.space5
                        verticalCenter: parent.verticalCenter
                    }
                    width: item.priority > 0 && !item.done ? Theme.smallGlyphSize : 0
                    height: Theme.smallGlyphSize
                    visible: width > 0
                    name: "flag"
                    color: Theme.priorityColor(item.priority)
                }
                Text {
                    id: dueLabel

                    anchors {
                        right: parent.right
                        rightMargin: Theme.space5
                        verticalCenter: parent.verticalCenter
                    }
                    visible: item.due !== ""
                    text: item.due
                    color: item.overdue ? Theme.danger : Theme.textFaint
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textSm
                    font.weight: item.overdue ? Theme.weightBold : Theme.weightStrong
                }
            }
        }
    }

    Rectangle {
        anchors {
            top: parent.top
            bottom: parent.bottom
            right: parent.right
        }
        width: 1
        color: Theme.hairline
    }

    // The day popup for a to-do double-clicked in the list, which writes to its note.
    TaskSchedule {
        id: scheduler

        objectName: "taskListSchedule"
        /// The to-do it is for: a checkbox or a task note, where, and its words as written.
        property string kind
        property string path
        property int line
        property string written

        parent: Overlay.overlay
        onChosen: line => {
            if (kind === "note")
                Vault.scheduleTaskNote(path, line)
            else
                Vault.setTaskText(path, scheduler.line, written, line)
        }
    }
}
