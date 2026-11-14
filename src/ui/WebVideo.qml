import Jot.Ui
import QtQuick
import QtQuick.Controls

/// A video on the web, in the room its line keeps: a card with its site, its link and a play
/// button. Nothing is fetched from the site until play is pressed, so reading a note never
/// reaches the web by itself. Pressed, the site's player loads in its place, when Jot is built
/// with Qt WebEngine, or the video opens in the browser. It plays on while the note is scrolled
/// or edited, and goes back to its card when another note is shown.
Rectangle {
    id: root

    /// The site's name, as YouTube.
    property string site
    /// The link as written in the note.
    property url link
    /// The site's player for it.
    property url embed
    /// The site's player is loaded, in place of the card.
    property bool playing: false
    /// The player fills the window, as its fullscreen button asks; the player sets it.
    property bool fullScreen: false

    /// Asks the player to leave the whole window, as Escape does.
    signal exitFullScreenRequested

    function play() {
        if (Support.webVideo)
            root.playing = true
        else
            Qt.openUrlExternally(root.link)
    }

    function stop() {
        root.playing = false
    }

    // Another video on the same line, as after an edit, starts from its card.
    onEmbedChanged: stop()

    Connections {
        target: Vault
        function onCurrentChanged() {
            root.stop()
        }
    }

    radius: Theme.radiusLg
    color: Theme.surfaceAlt
    border.color: Theme.border
    clip: true

    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.space6
        spacing: Theme.space3
        visible: !root.playing

        StickerButton {
            objectName: "webVideoPlay"
            anchors.horizontalCenter: parent.horizontalCenter
            accent: true
            glyph: "play"
            Accessible.name: qsTr("Play on %1").arg(root.site)
            tip: Support.webVideo ? Accessible.name : qsTr("Open on %1").arg(root.site)
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

    Loader {
        id: player

        objectName: "webVideoPlayer"
        // In its line's room, or over the whole window when its fullscreen button asks.
        parent: root.fullScreen && root.Window.window ? root.Window.window.contentItem : root
        anchors.fill: parent
        z: root.fullScreen ? 1000 : 0
        active: root.playing && Support.webVideo
        // In the module only when built with Qt WebEngine, so loaded by name and told of the
        // card rather than read from.
        source: "WebVideoPlayer.qml"
        onLoaded: item.card = root
    }

    Shortcut {
        sequence: "Esc"
        enabled: root.fullScreen
        onActivated: root.exitFullScreenRequested()
    }
}
