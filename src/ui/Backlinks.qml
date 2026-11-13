pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// Under a note: the lines in other notes that link to it, and the notes that name it
/// without a link. Each opens its note, and each list folds under its heading.
Column {
    id: root

    visible: Vault.backlinks.linked + Vault.backlinks.unlinked > 0
    spacing: Theme.space2

    // Folds the list under it, open to begin with. One with nothing under it does not fold.
    component Heading: AbstractButton {
        id: heading

        property bool open: true
        property bool folds: true

        // Sized by what it says: a bare AbstractButton has no size of its own on older Qt.
        implicitWidth: implicitContentWidth + leftPadding + rightPadding
        implicitHeight: implicitContentHeight + topPadding + bottomPadding
        topPadding: Theme.space5
        bottomPadding: Theme.space2
        enabled: folds
        hoverEnabled: true
        focusPolicy: Qt.NoFocus
        Accessible.name: text
        Accessible.checkable: folds
        Accessible.checked: open
        onClicked: open = !open

        HoverHandler {
            enabled: heading.folds
            cursorShape: Qt.PointingHandCursor
        }

        contentItem: Row {
            spacing: Theme.space2

            Glyph {
                anchors.verticalCenter: parent.verticalCenter
                visible: heading.folds
                name: heading.open ? "chevron-down" : "chevron-right"
                color: heading.hovered ? Theme.textMuted : Theme.textFaint
                stroke: Theme.smallGlyphStroke
                implicitWidth: Theme.smallGlyphSize
                implicitHeight: Theme.smallGlyphSize
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: heading.text
                color: heading.hovered ? Theme.textMuted : Theme.textFaint
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textXs
                font.weight: Theme.weightBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0.6
            }
        }
        background: null
    }

    component Entry: AbstractButton {
        id: entry

        required property string path
        required property string title
        required property string excerpt
        required property bool mention

        width: root.width
        height: content.implicitHeight + 2 * Theme.space3
        hoverEnabled: true
        Accessible.name: title
        onClicked: Vault.current = path

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }
        background: Rectangle {
            radius: Theme.radiusMd
            color: entry.hovered ? Theme.surfaceAlt : Theme.surface
            border.color: Theme.hairline
        }

        contentItem: Column {
            id: content

            leftPadding: Theme.space4
            rightPadding: Theme.space4
            topPadding: Theme.space3
            spacing: Theme.space1

            Text {
                width: entry.width - 2 * Theme.space4
                text: entry.title
                elide: Text.ElideRight
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textMd
                font.weight: Theme.weightBold
            }
            Text {
                width: entry.width - 2 * Theme.space4
                visible: entry.excerpt !== ""
                text: entry.excerpt
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                maximumLineCount: 3
                elide: Text.ElideRight
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSm
            }
        }
    }

    Rectangle {
        width: root.width
        height: 1
        color: Theme.hairline
    }

    Heading {
        id: linkedHeading

        objectName: "linkedHeading"
        folds: Vault.backlinks.linked > 0
        text: Vault.backlinks.linked === 0 ? qsTr("No links to this note") : Vault.backlinks.linked
                                             === 1 ? qsTr("1 link to this note") : qsTr(
                                                         "%1 links to this note").arg(
                                                         Vault.backlinks.linked)
    }

    Repeater {
        model: Vault.backlinks

        Entry {
            visible: !mention && linkedHeading.open
        }
    }

    Heading {
        id: mentionedHeading

        objectName: "mentionedHeading"
        visible: Vault.backlinks.unlinked > 0
        text: qsTr("Mentioned without a link")
    }

    Repeater {
        model: Vault.backlinks

        Entry {
            visible: mention && mentionedHeading.open
        }
    }
}
