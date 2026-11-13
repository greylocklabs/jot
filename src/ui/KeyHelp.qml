pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// Every keyboard shortcut, from the table the window's shortcuts are made from, in tabs:
/// those that work anywhere, those for writing, and vi mode's. A shortcut that can change is
/// changed here, kept in Settings.
Popup {
    id: root

    /// KeyRouter's actions: {id, label, scope, keys, vi, leader, editable, custom} for each.
    required property var actions
    /// "general", "editor" or "vi".
    property string tab: "general"
    /// The action whose new shortcut is being pressed, or "".
    property string recording: ""
    /// Why the keys pressed were not taken, or "".
    property string message: ""

    /// What a row's key caps say: its keys, "Press keys" while recording, or "None".
    function capsFor(row) {
        if (recording === row.id)
            return [qsTr("Press keys")]
        if (row.keys.length === 0 && row.editable === true)
            return [qsTr("None")]
        return row.keys
    }

    /// Starts listening for the new shortcut of `id`.
    function record(id) {
        message = ""
        recording = id
        recorder.forceActiveFocus()
    }

    /// Takes a key press as the shortcut being recorded, when it can be one.
    function take(event) {
        event.accepted = true
        if (event.key === Qt.Key_Escape) {
            recording = ""
            message = ""
            return
        }
        if (event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete) {
            Settings.setKeys(recording, "")
            recording = ""
            message = ""
            return
        }
        const sequence = router.sequenceFrom(event.key, event.modifiers)
        if (sequence === "")
            return
        const held = Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier
        const functionKey = event.key >= Qt.Key_F1 && event.key <= Qt.Key_F35
        if ((event.modifiers & held) === 0 && !functionKey) {
            message = qsTr("Add Ctrl or Alt to %1, so the key still types.").arg(sequence)
            return
        }
        const taken = router.conflict(recording, sequence)
        if (taken !== "") {
            message = qsTr("%1 is already %2.").arg(sequence).arg(taken)
            return
        }
        Settings.setKeys(recording, sequence)
        recording = ""
        message = ""
    }

    /// The actions of `scope` that have keys of `kind` ("keys", "vi" or "leader"), as
    /// {id, keys, label, editable, custom, defaultKeys} rows. With `kind` "keys", those that
    /// can be given keys are listed too.
    function rows(scope, kind) {
        const found = []
        for (const action of actions) {
            const editable = kind === "keys" && action.editable
            if ((scope === "" || action.scope === scope) && (action[kind].length > 0 || editable))
                found.push({
                               "id": action.id,
                               "keys": kind === "keys" ? action.keys : [action[kind]],
                               "label": action.label,
                               "editable": editable,
                               "custom": editable && action.custom,
                               "defaultKeys": action.defaultKeys
                           })
        }
        return found
    }

    anchors.centerIn: Overlay.overlay
    width: 620
    // Steady across tabs. The overlay, not the parent: in a LazyPopup the parent is a
    // loader with no size.
    height: Math.min(640, (Overlay.overlay ? Overlay.overlay.height : 800) - 2 * Theme.space6)
    padding: Theme.space6
    modal: true
    focus: true
    // Escape stops a recording rather than closing.
    closePolicy: recording !== "" ? Popup.CloseOnPressOutside : Popup.CloseOnEscape
                                    | Popup.CloseOnPressOutside
    onClosed: {
        recording = ""
        message = ""
    }

    Overlay.modal: Scrim {}

    background: Rectangle {
        radius: Theme.radiusXl
        color: Theme.surface
        border.color: Theme.border

        SoftShadow {
            anchors.fill: parent
            radius: parent.radius
        }
    }

    KeyRouter {
        id: router
    }

    contentItem: Column {
        spacing: Theme.space4

        // Takes the keys while a shortcut is recorded, before the window's shortcuts do.
        Item {
            id: recorder

            objectName: "keyRecorder"
            width: 0
            height: 0
            Keys.onShortcutOverride: event => event.accepted = root.recording !== ""
            Keys.onPressed: event => {
                if (root.recording !== "")
                    root.take(event)
            }
        }

        Item {
            width: parent.width
            height: title.implicitHeight

            Text {
                id: title
                text: qsTr("Keyboard shortcuts")
                color: Theme.text
                font.family: Theme.displayFontFamily
                font.pixelSize: Theme.textXl
                font.weight: Theme.weightBold
            }
            CloseButton {
                anchors {
                    right: parent.right
                    verticalCenter: title.verticalCenter
                }
                onClicked: root.close()
            }
        }

        Row {
            spacing: Theme.space2

            Repeater {
                model: [
                    {
                        "id": "general",
                        "label": qsTr("General")
                    },
                    {
                        "id": "editor",
                        "label": qsTr("Editor")
                    },
                    {
                        "id": "vi",
                        "label": qsTr("Vi mode")
                    }
                ]

                PillButton {
                    id: tabButton
                    required property var modelData
                    label: tabButton.modelData.label
                    selected: root.tab === tabButton.modelData.id
                    onClicked: root.tab = tabButton.modelData.id
                }
            }
        }

        Flickable {
            width: parent.width
            height: root.availableHeight - y
            contentHeight: content.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            Column {
                id: content
                width: parent.width
                spacing: Theme.space4

                Text {
                    objectName: "keyMessage"
                    visible: root.tab !== "vi"
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: root.message !== "" ? root.message : root.recording !== "" ? qsTr(
                                                                                           "Press the new shortcut: Backspace for none, Esc to keep it.") :
                                                                                       qsTr("Click a shortcut to change it.")
                    color: root.message !== "" ? Theme.danger : Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textSm
                }
                Table {
                    visible: root.tab === "general"
                    title: qsTr("Anywhere")
                    rows: root.rows("window", "keys").concat(root.rows("list", "keys"))
                }

                Table {
                    visible: root.tab === "editor"
                    title: qsTr("While writing")
                    rows: root.rows("note", "keys")
                }

                Text {
                    visible: root.tab === "vi"
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: Settings.viMode ? qsTr(
                                                "Vi mode is on. A note opens in normal mode, where keys move and edit; i or a starts typing and Esc stops.") :
                                            qsTr("Vi mode is off; turn it on under Settings, Keyboard.")
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textSm
                }
                Table {
                    visible: root.tab === "vi"
                    title: qsTr("Out of the note")
                    rows: root.rows("window", "vi")
                }
                Table {
                    visible: root.tab === "vi"
                    title: qsTr("In the note, normal mode")
                    rows: root.rows("note", "vi")
                }
                Table {
                    visible: root.tab === "vi"
                    title: qsTr("In the note, visual mode")
                    rows: root.rows("visual", "vi").concat([
                                                               {
                                                                   "keys": ["h", "j", "k", "l"],
                                                                   "label": qsTr(
                                                                                "Select, by normal mode's moves")
                                                               }
                                                           ])
                }
                Table {
                    objectName: "viCommands"
                    visible: root.tab === "vi"
                    title: Settings.leaderKey === ":" ? qsTr("Commands, after Space") : qsTr(
                                                            "Commands, after :")
                    rows: [
                        {
                            "keys": [":N"],
                            "label": qsTr("Go to line N; :0 and :1 the top, :$ the end")
                        },
                        {
                            "keys": [":w"],
                            "label": qsTr("Save")
                        },
                        {
                            "keys": [":q"],
                            "label": qsTr("Close the tab")
                        },
                        {
                            "keys": [":wq"],
                            "label": qsTr("Save and close the tab")
                        },
                        {
                            "keys": [":s/old/new/"],
                            "label": qsTr("Replace on the line; g for every match, i for any case")
                        },
                        {
                            "keys": [":%s/old/new/g"],
                            "label": qsTr("Replace in the whole note, as one step to undo")
                        },
                        {
                            "keys": [":noh"],
                            "label": qsTr("Clear the find highlights")
                        }
                    ]
                }
                Table {
                    visible: root.tab === "vi"
                    title: qsTr("After the leader (%1)").arg(Settings.leaderKey === " " ? qsTr(
                                                                                              "Space") :
                                                                                          Settings.leaderKey)
                    rows: root.rows("", "leader")
                }
            }
        }
    }

    /// A titled list of keys and what they do, in two columns.
    component Table: Column {
        id: table

        required property string title
        required property var rows

        width: parent.width
        spacing: Theme.space2

        Text {
            text: table.title
            color: Theme.textFaint
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
            font.weight: Theme.weightHeavy
        }
        Grid {
            columns: 2
            columnSpacing: Theme.space5
            rowSpacing: Theme.space2

            Repeater {
                model: table.rows

                Row {
                    id: row
                    required property var modelData
                    width: (table.width - Theme.space5) / 2
                    spacing: Theme.space3

                    Row {
                        id: caps
                        spacing: Theme.space1

                        Repeater {
                            model: root.capsFor(row.modelData)

                            KeyCap {
                                required property string modelData
                                keys: modelData
                                opacity: row.modelData.editable === true
                                         && row.modelData.keys.length === 0 && root.recording
                                         !== row.modelData.id ? 0.6 : 1
                            }
                        }

                        // A shortcut that can change is changed by clicking it.
                        TapHandler {
                            enabled: row.modelData.editable === true
                            onTapped: root.record(row.modelData.id)
                        }
                        HoverHandler {
                            enabled: row.modelData.editable === true
                            cursorShape: Qt.PointingHandCursor
                        }
                        objectName: row.modelData.editable === true ? "keys_" + row.modelData.id :
                                                                      ""

                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: row.width - caps.width - row.spacing - (reset.visible ? reset.width
                                                                                       + row.spacing :
                                                                                       0)
                        elide: Text.ElideRight
                        text: row.modelData.label
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                    PillButton {
                        id: reset

                        objectName: "reset_" + row.modelData.id
                        anchors.verticalCenter: parent.verticalCenter
                        visible: row.modelData.custom === true
                        label: qsTr("Reset")
                        tip: (row.modelData.defaultKeys || []).length > 0 ? qsTr("Back to %1").arg(
                                                                                row.modelData.defaultKeys.join(
                                                                                    ", ")) : qsTr(
                                                                                "Back to no shortcut")
                        onClicked: Settings.resetKeys(row.modelData.id)
                    }
                }
            }
        }
    }
}
