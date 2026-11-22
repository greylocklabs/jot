pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// A note type's name, look, template and properties as a form, each change written to its file
/// at once.
Column {
    id: root

    /// The type's id, its file's name.
    required property string typeId
    /// The type as its file has it now, read again whenever the vault's types change.
    readonly property var form: Vault.noteTypes.length >= 0 ? Vault.typeForm(typeId) : ({})
    /// The icons a type can take.
    readonly property var icons: ["note", "person", "people", "pin", "box", "flag", "video", "bell", "clock",
        "mic", "task", "table", "quote", "braces", "search", "play", "sound", "list-bullet"]

    readonly property bool editable: form.editable !== false

    /// Asks for the type's file to be shown as YAML, which the dialog does by closing first.
    signal yamlRequested

    /// Writes `form` with `change` made to a copy of it.
    function edit(change) {
        const copy = JSON.parse(JSON.stringify(root.form))
        change(copy)
        Vault.saveTypeForm(root.typeId, copy)
    }

    objectName: "typeEditor"
    spacing: Theme.space3

    // A file with more than the form shows is edited as YAML, so nothing in it is lost.
    Row {
        visible: root.form.editable === false
        spacing: Theme.space3

        Text {
            objectName: "typeNotEditable"
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, root.width - yamlButton.width - parent.spacing)
            wrapMode: Text.Wrap
            text: qsTr(
                      "This type's file has comments or lines the form cannot show, so edit it as YAML to keep them.")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
        }
        PillButton {
            id: yamlButton

            anchors.verticalCenter: parent.verticalCenter
            label: qsTr("Edit as YAML")
            onClicked: root.yamlRequested()
        }
    }

    component Label: Text {
        width: 100
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textSm
    }

    Row {
        enabled: root.editable
        spacing: Theme.space3

        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Name")
        }
        Field {
            objectName: "typeName"
            width: 220
            text: root.form.name ?? ""
            Accessible.name: qsTr("Type name")
            onEditingFinished: {
                if (text.trim() !== "" && text !== root.form.name)
                    root.edit(form => form.name = text.trim())
            }
        }
    }
    // The icons as buttons to pick from, as the emoji are.
    Row {
        enabled: root.editable
        spacing: Theme.space3

        Label {
            text: qsTr("Icon")
        }
        Flow {
            width: root.width - 100 - Theme.space3
            spacing: Theme.space1

            Repeater {
                model: root.icons

                AbstractButton {
                    id: iconButton

                    required property string modelData
                    readonly property bool chosen: (root.form.icon || "note") === modelData

                    objectName: "typeIcon"
                    width: 32
                    height: 32
                    hoverEnabled: true
                    Accessible.name: modelData
                    onClicked: root.edit(form => form.icon = iconButton.modelData)

                    background: Rectangle {
                        radius: Theme.radiusSm
                        color: iconButton.chosen ? Theme.tint(Theme.accent, 0.25) :
                                                   iconButton.hovered ? Theme.surfaceAlt :
                                                                        "transparent"
                        border.width: iconButton.chosen ? 1 : 0
                        border.color: Theme.accent
                    }
                    contentItem: Item {
                        Glyph {
                            anchors.centerIn: parent
                            name: iconButton.modelData
                            color: Theme.typeColor(root.form.color ?? "")
                        }
                    }
                }
            }
        }
    }
    Row {
        enabled: root.editable
        spacing: Theme.space3

        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Color")
        }
        Repeater {
            model: [""].concat(Vault.typeColorNames())

            AbstractButton {
                id: swatch

                required property string modelData
                readonly property bool chosen: (root.form.color ?? "") === modelData

                objectName: "typeColor"
                width: 24
                height: 24
                hoverEnabled: true
                Accessible.name: modelData !== "" ? modelData : qsTr("No color")
                onClicked: root.edit(form => form.color = swatch.modelData)

                background: Rectangle {
                    radius: width / 2
                    color: swatch.modelData !== "" ? Theme.typeColor(swatch.modelData) :
                                                     "transparent"
                    border.width: swatch.chosen ? 2 : 1
                    border.color: swatch.chosen ? Theme.text : Theme.border
                }
            }
        }
    }
    Row {
        enabled: root.editable
        spacing: Theme.space3

        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Builds on")
        }
        Dropdown {
            objectName: "typeExtends"
            minimumWidth: 220
            model: [
                {
                    "id": "",
                    "name": qsTr("No other type")
                }
            ].concat(Vault.noteTypes.filter(type => type.id !== root.typeId).map(type => ({
                "id": type.name,
                "name": type.name
            })))
            currentValue: root.form.extends ?? ""
            Accessible.name: qsTr("Type it builds on")
            onActivated: value => root.edit(form => form.extends = value)
        }
    }
    Row {
        enabled: root.editable
        spacing: Theme.space3

        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Template")
        }
        Dropdown {
            objectName: "typeTemplate"
            minimumWidth: 220
            model: [
                {
                    "id": "",
                    "name": qsTr("No template")
                }
            ].concat(Vault.templates().map(each => ({
                "id": each.path,
                "name": each.title
            })))
            currentValue: root.form.template ?? ""
            Accessible.name: qsTr("Template")
            onActivated: value => root.edit(form => form.template = value)
        }
    }

    Label {
        text: qsTr("Properties")
    }
    Repeater {
        model: root.form.properties ?? []

        Item {
            id: property
            enabled: root.editable

            required property var modelData
            required property int index

            /// Changes this property in a copy of the form and writes it.
            function set(key, value) {
                root.edit(form => form.properties[property.index][key] = value)
            }

            objectName: "typeProperty"
            width: root.width
            height: propertyFields.implicitHeight

            Row {
                id: propertyFields

                spacing: Theme.space2

                Field {
                    width: 140
                    text: property.modelData.key
                    placeholderText: qsTr("Key")
                    Accessible.name: qsTr("Property key")
                    onEditingFinished: {
                        if (text.trim() !== "" && text !== property.modelData.key)
                            property.set("key", text.trim())
                    }
                }
                Dropdown {
                    objectName: "propertyKind"
                    minimumWidth: 120
                    model: Vault.propertyKinds().map(kind => ({
                        "id": kind,
                        "name": kind
                    }))
                    currentValue: property.modelData.kind
                    Accessible.name: qsTr("Kind of %1").arg(property.modelData.key)
                    onActivated: value => property.set("kind", value)
                }
                CheckBox {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("List")
                    checked: property.modelData.list
                    onClicked: property.set("list", checked)
                }
                Field {
                    visible: property.modelData.kind === "choice"
                    width: 200
                    text: property.modelData.choices
                    placeholderText: qsTr("Choices, with commas between")
                    Accessible.name: qsTr("Choices for %1").arg(property.modelData.key)
                    onEditingFinished: {
                        if (text !== property.modelData.choices)
                            property.set("choices", text)
                    }
                }
                Dropdown {
                    visible: property.modelData.kind === "link"
                    minimumWidth: 160
                    model: [
                        {
                            "id": "",
                            "name": qsTr("Any note")
                        }
                    ].concat(Vault.noteTypes.map(type => ({
                        "id": type.name,
                        "name": type.name
                    })))
                    currentValue: property.modelData.linkType
                    Accessible.name: qsTr("Type %1 links to").arg(property.modelData.key)
                    onActivated: value => property.set("linkType", value)
                }
            }
            // On the right, as the type's own trash is.
            IconButton {
                objectName: "removeTypeProperty"
                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                }
                glyph: "trash"
                Accessible.name: qsTr("Remove %1").arg(property.modelData.key)
                onClicked: root.edit(form => form.properties.splice(property.index, 1))
            }
        }
    }
    Row {
        enabled: root.editable
        spacing: Theme.space3

        Field {
            id: newKey

            objectName: "newTypeProperty"
            width: 140
            placeholderText: qsTr("New property")
            onAccepted: addProperty.clicked()
        }
        StickerButton {
            id: addProperty

            anchors.verticalCenter: parent.verticalCenter
            enabled: newKey.text.trim() !== ""
            text: qsTr("Add")
            onClicked: {
                const key = newKey.text.trim()
                newKey.text = ""
                root.edit(form => form.properties.push({
                                                           "key": key,
                                                           "kind": "text",
                                                           "list": false,
                                                           "choices": "",
                                                           "linkType": ""
                                                       }))
            }
        }
        PillButton {
            objectName: "editTypeYaml"
            anchors.verticalCenter: parent.verticalCenter
            label: qsTr("Edit as YAML")
            onClicked: root.yamlRequested()
        }
    }
}
