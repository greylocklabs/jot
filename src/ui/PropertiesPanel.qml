pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// A note's properties as a form above its text, in place of the front matter they are written
/// in: its type, then its type's properties, then any other keys.
Item {
    id: root

    /// As MarkdownStyler::properties gives them.
    property var rows: []
    /// Shows the values without a way to change them, as Read does.
    property bool readOnly: false
    /// Also shows the type's properties the note leaves empty.
    property bool showEmpty: false
    /// Shows the fields for a key of the note's own.
    property bool adding: false
    readonly property int emptyCount: rows.filter(row => !row.present).length

    /// Whether the note has `key` already, which Add would write over.
    function taken(key) {
        return rows.some(row => row.present && row.key === key.trim())
    }

    /// A type was picked for the note, so the editor can offer its template.
    signal typePicked(string name)
    /// The front matter is to be edited as text, which Source shows.
    signal textRequested

    objectName: "propertiesPanel"
    implicitHeight: column.implicitHeight + Theme.space3

    Column {
        id: column

        width: root.width
        spacing: 0

        Repeater {
            model: root.rows.filter(row => row.present || (root.showEmpty && !root.readOnly))

            Item {
                id: row

                required property var modelData
                readonly property string key: modelData.key
                readonly property string kind: modelData.kind
                readonly property string value: modelData.value
                readonly property string problem: modelData.problem ?? ""

                /// Sets the property, as text, a list's items as text with commas, or a bool.
                function write(value) {
                    if (row.kind === "type")
                        root.typePicked(value)
                    else
                        Vault.setNoteProperty(row.key, value)
                }

                objectName: "propertyRow"
                width: column.width
                height: Math.max(Theme.listRowHeight, editor.implicitHeight) + (problemText.visible
                                                                                ? problemText.implicitHeight :
                                                                                  0) + Theme.space1

                HoverHandler {
                    id: rowHover
                }
                Text {
                    id: label

                    y: (Math.max(Theme.listRowHeight, editor.implicitHeight) - height) / 2
                    width: Math.min(120, row.width / 3)
                    text: row.key
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: row.problem !== "" ? Theme.danger : Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textSm
                }
                Loader {
                    id: editor

                    anchors {
                        left: label.right
                        leftMargin: Theme.space3
                        right: remove.left
                        rightMargin: Theme.space1
                    }
                    y: (Math.max(Theme.listRowHeight, implicitHeight) - implicitHeight) / 2
                    sourceComponent: {
                        if (root.readOnly || row.kind === "nested")
                            return shownValue
                        switch (row.kind) {
                        case "type":
                            return typeChoice
                        case "choice":
                            return row.modelData.list ? textValue : choice
                        case "checkbox":
                            return row.modelData.list ? textValue : checkbox
                        case "date":
                            return row.modelData.list ? textValue : datePick
                        default:
                            return textValue
                        }
                    }
                }
                // Takes the value away; a type's property stays, empty, to fill in later.
                StickerButton {
                    id: remove

                    objectName: "removeProperty"
                    anchors.right: parent.right
                    y: (Math.max(Theme.listRowHeight, editor.implicitHeight) - height) / 2
                    width: height
                    // Not for a nested value, which the panel leaves to Source.
                    readonly property bool shown: !root.readOnly && row.modelData.present && row.kind
                                                  !== "nested"

                    opacity: shown && (rowHover.hovered || activeFocus) ? 1 : 0
                    enabled: shown
                    glyph: "close"
                    Accessible.name: qsTr("Clear %1").arg(row.key)
                    onClicked: row.write("")
                }
                Text {
                    id: problemText

                    anchors {
                        left: editor.left
                        right: editor.right
                        top: editor.bottom
                    }
                    visible: row.problem !== ""
                    text: row.problem
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    color: Theme.danger
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textXs
                }

                Component {
                    id: shownValue

                    Text {
                        text: row.kind === "nested" ? qsTr(
                                                          "Set in the note's front matter, in Source") :
                                                      row.value !== "" ? row.value : "-"
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                        height: Theme.listRowHeight
                        color: row.value !== "" && row.kind !== "nested" ? Theme.text :
                                                                           Theme.textFaint
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textSm
                    }
                }
                Component {
                    id: typeChoice

                    Dropdown {
                        readonly property var types: Vault.noteTypes

                        objectName: "propertyType"
                        minimumWidth: 180
                        model: [
                            {
                                "id": "",
                                "name": qsTr("No type")
                            }
                        ].concat(types.map(type => ({
                            "id": type.name,
                            "name": type.name
                        })))
                        // A type written in another case still shows as the type it names.
                        currentValue: {
                            const written = row.value.trim().toLowerCase()
                            const type = types.find(each => each.name.toLowerCase() === written
                                                            || each.id.toLowerCase() === written)
                            return type ? type.name : row.value
                        }
                        Accessible.name: qsTr("Type")
                        onActivated: value => row.write(value)
                    }
                }
                Component {
                    id: choice

                    Dropdown {
                        /// The value as written, when it is none of the choices, so it shows.
                        readonly property bool stray: row.value !== "" && !row.modelData.choices.some(
                                                          each => each.toLowerCase()
                                                                  === row.value.toLowerCase())

                        objectName: "propertyChoice"
                        minimumWidth: 180
                        model: [
                            {
                                "id": "",
                                "name": qsTr("None")
                            }
                        ].concat(stray ? [
                                             {
                                                 "id": row.value,
                                                 "name": qsTr("%1 (not a choice)").arg(row.value)
                                             }
                                         ] : []).concat(row.modelData.choices.map(each => ({
                                             "id": each,
                                             "name": each
                                         })))
                        currentValue: stray ? row.value : row.modelData.choices.find(each => each.toLowerCase(
                                                                                                 ) === row.value.toLowerCase(
                                                                                                 )) ?? ""
                        Accessible.name: row.key
                        onActivated: value => row.write(value)
                    }
                }
                Component {
                    id: checkbox

                    CheckBox {
                        objectName: "propertyCheck"
                        checked: ["true", "yes"].includes(row.value.toLowerCase())
                        text: ""
                        Accessible.name: row.key
                        onClicked: row.write(checked)
                    }
                }
                Component {
                    id: datePick

                    DatePicker {
                        objectName: "propertyDate"
                        day: row.value !== "" ? Date.fromLocaleString(Qt.locale(), row.value.slice(0,
                                                                                                   10), "yyyy-MM-dd") :
                                                new Date(NaN)
                        placeholder: qsTr("Pick a day")
                        Accessible.name: row.key
                        onPicked: day => row.write(Qt.formatDate(day, "yyyy-MM-dd"))
                    }
                }
                Component {
                    id: textValue

                    Field {
                        id: field

                        /// What is being typed for a link: the last of a list's items.
                        readonly property string typed: text.slice(text.lastIndexOf(",") + 1).trim()

                        objectName: "propertyText"
                        text: row.value
                        placeholderText: row.kind === "link" && row.modelData.linkType !== "" ? qsTr(
                                                                                                    "A %1").arg(
                                                                                                    row.modelData.linkType) :
                                                                                                row.modelData.list
                                                                                                ? qsTr("Items, with commas between") :
                                                                                                  row.kind
                                                                                                  === "date"
                                                                                                  ? "2026-11-16" :
                                                                                                    qsTr("Empty")
                        validator: row.kind === "number" && !row.modelData.list ? number : null
                        Accessible.name: row.key
                        // Quiet until pointed at or typed in, so the values read as the note's.
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: field.activeFocus ? Theme.surface : field.hovered
                                                       ? Theme.surfaceAlt : "transparent"
                            border.width: field.activeFocus ? 1 : 0
                            border.color: Theme.accent
                        }
                        onEditingFinished: {
                            suggestions.items = []
                            if (text !== row.value)
                                row.write(text)
                        }
                        onTextEdited: suggestions.items = row.kind === "link" && typed !== "" ? Vault.frontMatterSuggestions(
                                                                                                    "value", row.key,
                                                                                                    typed) : []
                        Keys.onPressed: event => {
                            if (suggestions.items.length === 0)
                                return
                            if (event.key === Qt.Key_Down || event.key === Qt.Key_Up) {
                                const step = event.key === Qt.Key_Down ? 1 : -1
                                suggestions.highlighted = Math.max(0, Math.min(
                                                                       suggestions.items.length - 1,
                                                                       suggestions.highlighted
                                                                       + step))
                                event.accepted = true
                            } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                                       && suggestions.highlighted >= 0) {
                                suggestions.picked(suggestions.highlighted)
                                event.accepted = true
                            } else if (event.key === Qt.Key_Escape) {
                                suggestions.items = []
                                event.accepted = true
                            }
                        }

                        // As numbers are written in the file, whatever the desktop's language.
                        DoubleValidator {
                            id: number
                            locale: "C"
                            notation: DoubleValidator.StandardNotation
                        }
                        // A link offers notes of the type it takes.
                        SuggestionPopup {
                            id: suggestions

                            wanted: field.activeFocus
                            onPicked: index => {
                                const name = items[index].text.replace(/^"?\[\[/, "").replace(
                                          /\]\]"?$/, "")
                                const before = field.text.slice(0, field.text.lastIndexOf(",") + 1)
                                field.text = before + (before !== "" ? " " : "") + name
                                items = []
                                row.write(field.text)
                            }
                        }
                    }
                }
            }
        }
        Row {
            visible: !root.readOnly
            topPadding: Theme.space1
            spacing: Theme.space2

            PillButton {
                objectName: "showEmptyProperties"
                visible: root.emptyCount > 0
                label: root.showEmpty ? qsTr("Hide empty") : qsTr("Show %n empty", "",
                                                                  root.emptyCount)
                onClicked: root.showEmpty = !root.showEmpty
            }
            PillButton {
                objectName: "addPropertyShown"
                visible: !root.adding
                label: qsTr("Add a property")
                onClicked: {
                    root.adding = true
                    newKey.forceActiveFocus()
                }
            }
            PillButton {
                objectName: "editAsText"
                label: qsTr("Edit as text")
                onClicked: root.textRequested()
            }
        }
        // A key of the note's own, which no type names.
        Row {
            visible: !root.readOnly && root.adding
            topPadding: Theme.space2
            spacing: Theme.space2

            Field {
                id: newKey

                objectName: "newPropertyKey"
                width: Math.min(120, column.width / 3)
                placeholderText: qsTr("New property")
                onAccepted: newValue.forceActiveFocus()
                Keys.onEscapePressed: root.adding = false
            }
            Field {
                id: newValue

                objectName: "newPropertyValue"
                width: column.width - newKey.width - addProperty.width - 2 * parent.spacing
                placeholderText: qsTr("Its value")
                onAccepted: addProperty.clicked()
                Keys.onEscapePressed: root.adding = false
            }
            StickerButton {
                id: addProperty

                objectName: "addProperty"
                anchors.verticalCenter: parent.verticalCenter
                enabled: Vault.writable && newKey.text.trim() !== "" && newValue.text.trim() !== "" &&
                         !root.taken(newKey.text)
                text: qsTr("Add")
                onClicked: {
                    Vault.setNoteProperty(newKey.text.trim(), newValue.text)
                    newKey.text = ""
                    newValue.text = ""
                    root.adding = false
                }
            }
        }
        Text {
            objectName: "propertyTaken"
            visible: root.adding && root.taken(newKey.text)
            text: qsTr("The note has %1 already; change it in its row above.").arg(newKey.text.trim(
                                                                                       ))
            textFormat: Text.PlainText
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textXs
        }
    }
    // Where the properties end and the note begins.
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.hairline
    }
}
