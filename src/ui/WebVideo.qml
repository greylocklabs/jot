import Jot.Ui
import QtQuick
import QtQuick.Controls

/// A video on the web, in the room its line keeps: a card with its site, its link and a play
/// button. Nothing is fetched from the site until play is pressed, so reading a note never
/// reaches the web by itself.
Rectangle {
    id: root

    /// The site's name, as YouTube.
    property string site
    /// The link as written in the note.
    property url link
    /// The site's player for it.
    property url embed

    /// Plays the video: in the browser.
    function play() {
        Qt.openUrlExternally(root.link)
    }

    radius: Theme.radiusLg
    color: Theme.surfaceAlt
    border.color: Theme.border
    clip: true

    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.space6
        spacing: Theme.space3

        StickerButton {
            objectName: "webVideoPlay"
            anchors.horizontalCenter: parent.horizontalCenter
            accent: true
            glyph: "play"
            Accessible.name: qsTr("Play on %1").arg(root.site)
            tip: Accessible.name
            onClicked: root.play()
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            textFormat: Text.PlainText
            text: root.site
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textMd
            font.weight: Theme.weightBold
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideMiddle
            textFormat: Text.PlainText
            text: root.link.toString()
            color: Theme.textFaint
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
        }
    }
}
