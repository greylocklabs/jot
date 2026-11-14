import Jot.Ui
import QtQuick
import QtWebEngine

/// A web video's own player, from its site, as WebVideo loads it once play is pressed. It runs
/// in a private profile that keeps no cookies, cache or storage, is refused every permission,
/// and stays on its site: a link followed from it, as to the video's page, opens in the browser.
Item {
    id: root

    /// The card it plays in, which says what to play and is told when it fills the window.
    property WebVideo card
    readonly property url embed: card?.embed ?? ""

    // The card moves the player over the whole window while its fullscreen button asks.
    Binding {
        target: root.card
        property: "fullScreen"
        value: view.isFullScreen
        when: root.card !== null
    }

    Connections {
        target: root.card
        function onExitFullScreenRequested() {
            view.triggerWebAction(WebEngineView.ExitFullScreen)
        }
    }

    /// The host of `url`, as www.youtube-nocookie.com.
    function hostOf(url) {
        return String(url).split("/")[2] ?? ""
    }

    WebEngineProfile {
        id: privateProfile

        offTheRecord: true
        httpCacheType: WebEngineProfile.MemoryHttpCache
        persistentCookiesPolicy: WebEngineProfile.NoPersistentCookies
    }

    WebEngineView {
        id: view

        anchors.fill: parent
        profile: privateProfile
        // Play was just pressed, so it starts at once.
        url: String(root.embed) + (String(root.embed).includes("?") ? "&" : "?") + "autoplay=1"
        backgroundColor: "black"
        settings.playbackRequiresUserGesture: false
        settings.javascriptCanOpenWindows: false
        settings.localContentCanAccessFileUrls: false
        settings.localContentCanAccessRemoteUrls: false
        settings.pluginsEnabled: false
        settings.fullScreenSupportEnabled: true

        onNavigationRequested: request => {
            // The player's own pages, and what it loads inside itself, stay; the rest opens
            // in the browser.
            if (!request.isMainFrame || root.hostOf(request.url) === root.hostOf(root.embed)) {
                request.accept()
                return
            }
            request.reject()
            Qt.openUrlExternally(request.url)
        }
        onNewWindowRequested: request => Qt.openUrlExternally(request.requestedUrl)
        onFullScreenRequested: request => request.accept()
        onPermissionRequested: permission => permission.deny()
    }
}
