import Jot.Ui
import QtQuick
import QtQuick.Controls

/// Sends a problem report, or an idea, to the report service, which files it as
/// a public issue in greylocklabs/jot. What it sends is all on screen first.
Popup {
    id: root

    /// "edit" while filling it in, then "sending", "sent" or "failed".
    property string phase: "edit"
    property bool idea: false
    property string diagnostics
    property url issue
    property string error

    anchors.centerIn: Overlay.overlay
    width: 520
    padding: Theme.space6
    modal: true
    focus: true
    // Closing mid-send would lose the answer, and a resend files a second issue.
    closePolicy: phase === "sending" ? Popup.NoAutoClose : (Popup.CloseOnEscape
                                                            | Popup.CloseOnPressOutside)

    Overlay.modal: Scrim {}

    onAboutToShow: {
        phase = "edit"
        idea = false
        summary.text = ""
        note.text = ""
        include.checked = true
        showDiagnostics.checked = false
        diagnostics = ""
        error = ""
        Support.gatherDiagnostics()
    }

    function send() {
        phase = "sending"
        Support.sendReport(idea, summary.text, note.text, include.checked ? diagnostics : "")
    }

    Connections {
        target: Support

        function onDiagnosticsReady(text) {
            root.diagnostics = text
        }
        function onReportSent(issue) {
            if (root.phase !== "sending")
                return
            root.issue = issue
            root.phase = "sent"
        }
        function onReportFailed(error) {
            if (root.phase !== "sending")
                return
            root.error = error
            root.phase = "failed"
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

        // In the corner, clear of the title.
        CloseButton {
            anchors {
                top: parent.top
                right: parent.right
                margins: Theme.space4
            }
            enabled: root.phase !== "sending"
            onClicked: root.close()
        }
    }

    contentItem: Column {
        spacing: Theme.space4

        Column {
            // Room for the X in the corner.
            width: parent.width - Theme.space7
            spacing: Theme.space1

            Text {
                text: qsTr("Report a problem")
                color: Theme.text
                font.family: Theme.displayFontFamily
                font.pixelSize: Theme.textXl
                font.weight: Theme.weightBold
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                text: qsTr("It becomes a public issue on GitHub, in greylocklabs/jot.")
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSm
            }
        }

        Column {
            visible: root.phase !== "sent"
            width: parent.width
            spacing: Theme.space4

            Row {
                spacing: Theme.space2

                PillButton {
                    label: qsTr("Something's wrong")
                    selected: !root.idea
                    onClicked: root.idea = false
                }
                PillButton {
                    label: qsTr("An idea")
                    selected: root.idea
                    onClicked: root.idea = true
                }
            }
            Field {
                id: summary
                width: parent.width
                maximumLength: 100
                placeholderText: root.idea ? qsTr("The idea, in a few words") : qsTr(
                                                 "What's wrong, in a few words")
                Accessible.name: qsTr("Summary")
            }
            TextArea {
                id: note
                width: parent.width
                height: Math.max(implicitHeight, 120)
                placeholderText: root.idea ? qsTr("What it would let you do") : qsTr(
                                                 "What you did, what you expected, and what happened instead")
                placeholderTextColor: Theme.textFaint
                wrapMode: TextArea.Wrap
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textMd
                leftPadding: Theme.space4
                rightPadding: Theme.space4
                topPadding: Theme.space3
                bottomPadding: Theme.space3
                Accessible.name: qsTr("Details")
                onTextChanged: if (length > 4000)
                                   remove(4000, length)

                background: Rectangle {
                    radius: Theme.radiusLg
                    color: Theme.surfaceAlt
                    border.width: parent.activeFocus ? 2 : 1
                    border.color: parent.activeFocus ? Theme.accent : Theme.border
                }
            }
            Toggle {
                id: include
                width: parent.width
                text: qsTr("Include diagnostics")
                checked: true
            }
            Text {
                visible: include.checked
                width: parent.width
                wrapMode: Text.Wrap
                text: qsTr(
                          "Jot's version, your system, and the end of Jot's logs, which can name your notes folder and notes but never say what the notes hold. Email addresses are masked.")
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSm
            }
            Toggle {
                id: showDiagnostics
                visible: include.checked
                width: parent.width
                text: qsTr("Show what is sent")
            }
            ScrollView {
                id: scroller
                visible: include.checked && showDiagnostics.checked
                width: parent.width
                height: 180
                clip: true
                contentWidth: availableWidth

                Text {
                    width: scroller.availableWidth
                    text: root.diagnostics === "" ? qsTr("Gathering...") : root.diagnostics
                    wrapMode: Text.WrapAnywhere
                    color: Theme.textMuted
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.textSm
                }
            }
            Text {
                visible: root.phase === "failed"
                width: parent.width
                wrapMode: Text.Wrap
                text: root.error
                color: Theme.danger
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSm
            }
            Row {
                anchors.right: parent.right
                spacing: Theme.space3

                StickerButton {
                    visible: root.phase === "failed"
                    text: qsTr("Open on GitHub instead")
                    onClicked: {
                        Support.reportOnGitHub(root.idea, summary.text, note.text, include.checked
                                               ? root.diagnostics : "")
                        root.close()
                    }
                }
                StickerButton {
                    text: qsTr("Cancel")
                    enabled: root.phase !== "sending"
                    onClicked: root.close()
                }
                StickerButton {
                    accent: true
                    text: root.phase === "sending" ? qsTr("Sending...") : qsTr("Send")
                    enabled: root.phase !== "sending" && summary.text.trim() !== "" && (
                                 !include.checked || root.diagnostics !== "")
                    onClicked: root.send()
                }
            }
        }

        Column {
            visible: root.phase === "sent"
            width: parent.width
            spacing: Theme.space4

            Text {
                width: parent.width
                wrapMode: Text.Wrap
                text: qsTr("Thank you. It's filed, and you can follow it on GitHub.")
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textMd
            }
            Row {
                anchors.right: parent.right
                spacing: Theme.space3

                StickerButton {
                    text: qsTr("Open the issue")
                    onClicked: Qt.openUrlExternally(root.issue)
                }
                StickerButton {
                    accent: true
                    glyph: "check"
                    text: qsTr("Done")
                    onClicked: root.close()
                }
            }
        }
    }
}
