pragma ComponentBehavior: Bound
import Jot.Ui
import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

/// Writes a note out through Pandoc: as which file type, in which look, and where.
Popup {
    id: root

    /// The note to export.
    property string path
    property string format: "docx"
    property string preset: "standard"
    /// "", "exporting", "done" or "failed", with what to say about it.
    property string phase
    property string message

    readonly property bool pandoc: Vault.pandocFound()

    anchors.centerIn: Overlay.overlay
    width: 420
    padding: Theme.space6
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: {
        phase = ""
        message = ""
    }

    Overlay.modal: Scrim {}

    Connections {
        target: Vault
        function onExported(ok, message) {
            root.phase = ok ? "done" : "failed"
            root.message = ok ? qsTr("Saved as %1.").arg(message) : message
        }
    }

    background: Rectangle {
        radius: Theme.radiusXl
        color: Theme.surface
        border.color: Theme.border

        SoftShadow {
            anchors.fill: parent
            radius: parent.radius
        }

        CloseButton {
            anchors {
                top: parent.top
                right: parent.right
                margins: Theme.space4
            }
            onClicked: root.close()
        }
    }

    contentItem: Column {
        spacing: Theme.space4

        Text {
            width: parent.width - Theme.space7
            wrapMode: Text.Wrap
            text: qsTr("Export %1").arg(Vault.titleOf(root.path))
            color: Theme.text
            font.family: Theme.displayFontFamily
            font.pixelSize: Theme.textLg
            font.weight: Theme.weightBold
        }

        Text {
            visible: !root.pandoc
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTr(
                      "Export needs Pandoc, from your distribution's packages, such as pandoc on Fedora, Debian and Arch. PDFs also need a LaTeX engine, such as texlive.")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
        }

        Grid {
            visible: root.pandoc
            columns: 2
            columnSpacing: Theme.space3
            rowSpacing: Theme.space3
            verticalItemAlignment: Grid.AlignVCenter

            Text {
                text: qsTr("As")
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textMd
            }
            Dropdown {
                objectName: "exportFormat"
                minimumWidth: 240
                model: Vault.exportFormats().map(each => ({
                    "id": each.id,
                    "name": each.label
                }))
                currentValue: root.format
                Accessible.name: qsTr("File type")
                onActivated: value => root.format = value
            }
            Text {
                text: qsTr("Look")
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textMd
            }
            Dropdown {
                objectName: "exportPreset"
                minimumWidth: 240
                model: [
                    {
                        "id": "standard",
                        "name": qsTr("Pandoc's own")
                    },
                    {
                        "id": "manuscript",
                        "name": qsTr("Standard manuscript (Shunn)")
                    }
                ]
                currentValue: root.preset
                Accessible.name: qsTr("Look")
                onActivated: value => root.preset = value
            }
        }

        Text {
            visible: root.message !== ""
            width: parent.width
            wrapMode: Text.Wrap
            text: root.message
            color: root.phase === "failed" ? Theme.danger : Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
        }

        Row {
            spacing: Theme.space2

            StickerButton {
                text: root.phase === "done" ? qsTr("Close") : qsTr("Cancel")
                onClicked: root.close()
            }
            StickerButton {
                objectName: "exportButton"
                accent: true
                visible: root.pandoc
                enabled: root.phase !== "exporting"
                text: root.phase === "exporting" ? qsTr("Exporting...") : qsTr("Export...")
                onClicked: {
                    const format = Vault.exportFormats().find(each => each.id === root.format)
                    saveDialog.defaultSuffix = format.extension
                    saveDialog.nameFilters = [format.label + " (*." + format.extension + ")"]
                    // Named for the note, in Documents to begin with.
                    saveDialog.selectedFile = StandardPaths.writableLocation(
                                StandardPaths.DocumentsLocation) + "/" + encodeURIComponent(
                                Vault.titleOf(root.path)) + "." + format.extension
                    saveDialog.open()
                }
            }
        }
    }

    FileDialog {
        id: saveDialog

        title: qsTr("Export as")
        fileMode: FileDialog.SaveFile
        onAccepted: {
            root.phase = "exporting"
            root.message = ""
            Vault.exportNote(root.path, root.format, root.preset, selectedFile)
        }
    }
}
