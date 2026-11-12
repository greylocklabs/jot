pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// What Jot reads in a note's Markdown, from data/syntax.md, drawn as Read mode draws a note.
Popup {
    id: root

    anchors.centerIn: Overlay.overlay
    width: Math.min(680, (Overlay.overlay ? Overlay.overlay.width : 800) - 2 * Theme.space6)
    // The overlay, not the parent: in a LazyPopup the parent is a loader with no size.
    height: Math.min(720, (Overlay.overlay ? Overlay.overlay.height : 800) - 2 * Theme.space6)
    padding: Theme.space6
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

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

    contentItem: Item {
        Item {
            id: header

            width: parent.width
            height: title.implicitHeight

            Text {
                id: title
                text: qsTr("Syntax guide")
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

        ScrollView {
            id: scroller

            anchors {
                left: parent.left
                right: parent.right
                top: header.bottom
                topMargin: Theme.space4
                bottom: parent.bottom
            }
            contentWidth: availableWidth
            clip: true

            TextEdit {
                id: guide

                objectName: "syntaxGuide"
                width: scroller.availableWidth - Theme.space4
                readOnly: true
                selectByMouse: true
                // NoteReader draws the document itself; the TextEdit only shows it.
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                color: Theme.text
                selectionColor: Theme.tint(Theme.accent, 0.4)
                selectedTextColor: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textBase
                Accessible.name: title.text

                CodeBackdrops {
                    view: guide
                    runs: guideReader.codeRuns
                    padding: guideReader.codePadding
                }

                NoteReader {
                    id: guideReader

                    document: guide.textDocument
                    text: Support.syntaxGuide()
                    fontFamily: Theme.fontFamily
                    pixelSize: guide.font.pixelSize
                    width: guide.width
                }
            }
        }
    }
}
