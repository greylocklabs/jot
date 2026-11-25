pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// A date as a button that opens a month to pick a day from.
AbstractButton {
    id: root

    /// The day, as a local midnight.
    property date day
    /// Shown while there is no day.
    property string placeholder

    signal picked(date day)

    implicitHeight: Theme.listRowHeight + Theme.space2
    implicitWidth: caption.implicitWidth + 2 * Theme.space4
    Accessible.name: caption.text
    // A click while it is open closes it, as the button's own press leaves it open.
    onClicked: {
        if (calendar.opened) {
            calendar.close()
            return
        }
        month.today = new Date()
        month.month = isNaN(root.day.getTime()) ? new Date() : root.day
        calendar.open()
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: root.hovered || calendar.opened ? Theme.border : Theme.surfaceAlt
        border.width: root.visualFocus ? 2 : 0
        border.color: Theme.text
    }
    contentItem: Text {
        id: caption

        objectName: "dateCaption"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        text: isNaN(root.day.getTime()) ? root.placeholder : Qt.formatDate(root.day,
                                                                           "ddd, MMM d, yyyy")

        // No day yet reads as something to press, in the accent, not grayed out as if disabled.
        color: !root.enabled ? Theme.textFaint : isNaN(root.day.getTime()) ? Theme.accent :
                                                                             Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textMd
        font.weight: Theme.weightBold
    }

    MonthModel {
        id: month
        // The locale counts Sunday as 0, the model as 7.
        firstDay: Qt.locale().firstDayOfWeek || 7
    }

    Popup {
        id: calendar
        objectName: "calendar"
        y: root.height + Theme.space1
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        padding: Theme.space3
        onOpened: {
            typedDay.text = ""
            typedDay.forceActiveFocus()
        }

        background: Rectangle {
            radius: Theme.radiusLg
            color: Theme.surface
            border.color: Theme.border

            SoftShadow {
                anchors.fill: parent
                radius: parent.radius
            }
        }

        contentItem: Column {
            spacing: Theme.space2

            // A day in words, as @due takes them: the calendar goes to it, and Return picks it.
            Field {
                id: typedDay

                /// The day the words name, when they name one and nothing else.
                readonly property date found: {
                    if (text.trim() === "")
                        return new Date(NaN)
                    const read = Vault.readQuickTask(text)
                    return read.due !== "" && read.title === "" ? new Date(read.due.slice(0, 10)
                                                                           + "T00:00:00") : new Date(
                                                                      NaN)
                }

                objectName: "typedDay"
                width: 7 * Theme.space7
                placeholderText: qsTr("A day: fri, the 12th, 2w")
                Accessible.name: qsTr("A day in words")
                onFoundChanged: {
                    if (!isNaN(found.getTime()))
                        month.month = found
                }
                onAccepted: {
                    if (isNaN(found.getTime()))
                        return
                    calendar.close()
                    root.picked(found)
                }
            }

            Row {
                spacing: Theme.space2

                StickerButton {
                    glyph: "chevron-left"
                    Accessible.name: qsTr("Previous month")
                    onClicked: month.previous()
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 7 * Theme.space7 - 2 * Theme.space7 - 2 * parent.spacing
                    horizontalAlignment: Text.AlignHCenter
                    text: Qt.formatDate(month.month, "MMMM yyyy")
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textMd
                    font.weight: Theme.weightHeavy
                }
                StickerButton {
                    glyph: "chevron-right"
                    Accessible.name: qsTr("Next month")
                    onClicked: month.next()
                }
            }

            Grid {
                columns: 7

                Repeater {
                    model: month

                    AbstractButton {
                        id: cell

                        required property date date
                        required property int day
                        required property bool inMonth
                        required property bool isToday
                        readonly property bool chosen: date.getTime() === root.day.getTime()
                        readonly property bool typed: date.getTime() === typedDay.found.getTime()

                        width: Theme.space7
                        height: Theme.space7
                        Accessible.name: Qt.formatDate(date, Qt.locale().dateFormat(
                                                           Locale.LongFormat))
                        onClicked: {
                            calendar.close()
                            root.picked(date)
                        }

                        HoverHandler {
                            cursorShape: Qt.PointingHandCursor
                        }

                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: cell.chosen ? Theme.accent : cell.typed ? Theme.tint(Theme.accent,
                                                                                        0.3) : cell.hovered
                                                                             ? Theme.surfaceAlt :
                                                                               "transparent"

                            border.width: cell.isToday && !cell.chosen ? 1 : 0
                            border.color: Theme.accent
                        }
                        contentItem: Text {
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            text: cell.day
                            color: cell.chosen ? Theme.accentText : cell.inMonth ? Theme.text :
                                                                                   Theme.textFaint

                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textSm
                            font.weight: Theme.weightBold
                        }
                    }
                }
            }
        }
    }
}
