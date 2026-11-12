pragma ComponentBehavior: Bound
import Jot.Ui
import QtMultimedia
import QtQuick
import QtQuick.Controls

/// Plays a sound embedded in a note, in the room its line keeps: play or pause, where it is,
/// and how long it runs.
Rectangle {
    id: root

    property url source

    /// Minutes and seconds, as 1:05.
    function clock(ms) {
        const seconds = Math.floor(Math.max(0, ms) / 1000)
        return Math.floor(seconds / 60) + ":" + String(seconds % 60).padStart(2, "0")
    }

    radius: Theme.radiusLg
    color: Theme.surface
    border.color: Theme.border

    MediaPlayer {
        id: player
        source: root.source
        audioOutput: AudioOutput {
            id: sound
        }
    }

    Row {
        anchors {
            fill: parent
            leftMargin: Theme.space3
            rightMargin: Theme.space3
        }
        spacing: Theme.space2

        StickerButton {
            objectName: "playButton"
            anchors.verticalCenter: parent.verticalCenter
            glyph: player.playbackState === MediaPlayer.PlayingState ? "pause" : "play"
            Accessible.name: player.playbackState === MediaPlayer.PlayingState ? qsTr("Pause") :
                                                                                 qsTr("Play")
            tip: Accessible.name
            onClicked: player.playbackState === MediaPlayer.PlayingState ? player.pause() :
                                                                           player.play()
        }
        SeekBar {
            id: seek

            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - x - time.width - mute.width - 2 * parent.spacing
            from: 0
            to: Math.max(1, player.duration)
            value: player.position
            enabled: player.seekable
            Accessible.name: qsTr("Where in the recording")
            onMoved: player.position = value
        }
        Text {
            id: time

            anchors.verticalCenter: parent.verticalCenter
            text: root.clock(player.position) + " / " + root.clock(player.duration)
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSm
            font.features: {
                "tnum": 1
            }
        }
        StickerButton {
            id: mute

            objectName: "muteButton"
            anchors.verticalCenter: parent.verticalCenter
            glyph: sound.muted ? "muted" : "sound"
            Accessible.name: sound.muted ? qsTr("Sound on") : qsTr("Mute")
            tip: Accessible.name
            onClicked: sound.muted = !sound.muted
        }
    }
}
