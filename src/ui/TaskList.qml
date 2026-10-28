pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// The middle column when Tasks is chosen: the to-dos due today, coming up, with no day, or
/// all of them, narrowed to a label or a tag. Ticking one changes its note; choosing one opens
/// it there.
Rectangle {
    id: root

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

        ModeSwitch {
            objectName: "taskViews"
            readonly property var views: ["today", "upcoming", "anytime", "all"]

            options: [
                {
                    "label": qsTr("Today"),
                    "tip": qsTr("Overdue, and due today")
                },
                {
                    "label": qsTr("Upcoming"),
                    "tip": qsTr("Due after today, soonest first")
                },
                {
                    "label": qsTr("Anytime"),
                    "tip": qsTr("Not due on any day")
                },
                {
                    "label": qsTr("All"),
                    "tip": qsTr("Everything left to do")
                }
            ]
            current: Math.max(0, views.indexOf(Vault.tasks.view))
            onPicked: index => Vault.tasks.view = views[index]
        }

        Row {
            width: parent.width
            spacing: Theme.space3

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: Vault.tasks.count === 0 ? qsTr("Nothing here.") : Vault.tasks.count === 1 ? qsTr(
                                                                                                      "1 thing to do") :
                                                                                                  qsTr("%1 things to do").arg(
                                                                                                      Vault.tasks.count)
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textMd
                font.weight: Theme.weightStrong
            }
            // What the list is narrowed to, with a way to see it all again.
            PillButton {
                objectName: "taskFilter"
                anchors.verticalCenter: parent.verticalCenter
                visible: Vault.tasks.label !== "" || Vault.tasks.tag !== ""
                label: [Vault.tasks.label !== "" ? "@" + Vault.tasks.label : "", Vault.tasks.tag
                    !== "" ? "#" + Vault.tasks.tag : ""].filter(part => part !== "").join(" ")
                selected: true
                tip: qsTr("Show every to-do again")
                Accessible.name: qsTr("Show every to-do again")
                onClicked: {
                    Vault.tasks.label = ""
                    Vault.tasks.tag = ""
                }
            }
        }
    }

    ListView {
        id: list

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
        ScrollBar.vertical: ScrollBar {}

        section.property: "group"
        section.criteria: ViewSection.FullString
        section.delegate: Text {
            required property string section

            width: ListView.view.width
            leftPadding: Theme.space5
            topPadding: Theme.space4
            bottomPadding: Theme.space1
            text: section
            elide: Text.ElideRight
            color: Theme.textFaint
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textXs
            font.weight: Theme.weightBold
            font.capitalization: Font.AllUppercase
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
            required property var labels
            required property bool repeats

            width: ListView.view.width
            height: Math.max(Theme.listRowHeight, words.implicitHeight + 2 * Theme.space2)
            hoverEnabled: true
            Accessible.name: title
            onClicked: Vault.openAt(path, line)

            background: Rectangle {
                anchors.fill: parent
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space3
                radius: Theme.radiusSm
                color: item.hovered ? Theme.surfaceAlt : Theme.bg
            }

            contentItem: Item {
                AbstractButton {
                    id: box

                    x: Theme.space5
                    anchors.verticalCenter: parent.verticalCenter
                    width: Theme.textBase + Theme.space1
                    height: width
                    enabled: Vault.writable
                    Accessible.name: qsTr("Done")
                    Accessible.role: Accessible.CheckBox
                    onClicked: item.kind === "note" ? Vault.completeTask(item.path) : Vault.setTaskDone(
                                                          item.path, item.line, true)

                    background: Rectangle {
                        radius: Theme.radiusSm
                        color: box.hovered ? Theme.tint(Theme.accent, 0.25) : Theme.surface
                        border.color: box.hovered ? Theme.accent : Theme.border
                        border.width: 2
                    }
                }
                Column {
                    id: words

                    anchors {
                        left: box.right
                        leftMargin: Theme.space4
                        right: dueLabel.left
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
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textBase
                        font.weight: item.kind === "note" ? Theme.weightBold : Theme.weightStrong
                    }
                    // Where it is written, unless the heading says so, its labels, and whether
                    // it repeats.
                    Text {
                        width: parent.width
                        visible: text !== ""
                        text: [item.repeats ? "🔁" : "", item.note !== "" && item.note
                            !== item.ListView.section ? item.note : ""].concat(item.labels.map(
                                                                                   label => "@"
                                                                                            + label)).filter(
                            part => part !== "").join("  ")
                        elide: Text.ElideRight
                        color: Theme.textFaint
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
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
}
