pragma ComponentBehavior: Bound
import Jot.Ui
import QtMultimedia
import QtQuick
import QtQuick.Controls

/// Plays a video embedded in a note, in the room its line keeps: its first frame until played,
/// then play or pause, where it is, how long it runs, and the sound on or off. It never plays
/// by itself.
Rectangle {
    id: root

    property url source
    /// Whether it is playing, for tests and the play button.
    readonly property bool playing: player.playbackState === MediaPlayer.PlayingState

    /// Minutes and seconds, as 1:05.
    function clock(ms) {
        const seconds = Math.floor(Math.max(0, ms) / 1000)
        return Math.floor(seconds / 60) + ":" + String(seconds % 60).padStart(2, "0")
    }

    radius: Theme.radiusLg
    color: Theme.surface
    border.color: Theme.border
    clip: true

    MediaPlayer {
        id: player

        source: root.source
        videoOutput: output
        audioOutput: AudioOutput {
            id: sound
        }
        // Paused at the start once loaded, so its first frame shows in place of nothing.
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.LoadedMedia && playbackState
                    === MediaPlayer.StoppedState)
                pause()
        }
    }

    // The picture inset from the rounded edge, on a ground of its own, so its square corners
    // stay inside.
    Rectangle {
        id: screen

        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            bottom: controls.top
            margins: Theme.space2
            bottomMargin: Theme.space1
        }
        radius: Theme.radiusMd
        color: Theme.surfaceAlt
    }

    VideoOutput {
        id: output

        objectName: "videoOutput"
        anchors {
            fill: screen
            margins: Theme.space1
        }
        fillMode: VideoOutput.PreserveAspectFit

        // A click on the picture plays or pauses it, as the button does.
        TapHandler {
            onTapped: root.playing ? player.pause() : player.play()
        }
    }

    Row {
        id: controls

        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            leftMargin: Theme.space3
            rightMargin: Theme.space3
            bottomMargin: Theme.space2
        }
        height: playButton.height
        spacing: Theme.space2

        StickerButton {
            id: playButton

            objectName: "playButton"
            anchors.verticalCenter: parent.verticalCenter
            glyph: root.playing ? "pause" : "play"
            Accessible.name: root.playing ? qsTr("Pause") : qsTr("Play")
            tip: Accessible.name
            onClicked: root.playing ? player.pause() : player.play()
        }
        SeekBar {
            id: seek

            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - x - time.width - mute.width - 2 * parent.spacing
            from: 0
            to: Math.max(1, player.duration)
            value: player.position
            enabled: player.seekable
            Accessible.name: qsTr("Where in the video")
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
