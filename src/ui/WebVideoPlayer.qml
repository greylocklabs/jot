import Jot.Ui
import QtQuick
import QtWebEngine

/// A web video's own player, from its site, as WebVideo loads it once play is pressed. It runs
/// in a private profile that keeps no cookies, cache or storage, is refused every permission,
/// and stays on its site: a link followed from it, as to the video's page, opens in the browser.
/// The player sits in a page of Jot's own, as on a web page, since YouTube plays an embed only
/// for a page it can name.
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

    /// Where Jot's page with the player says it is, which the site sees as the page embedding
    /// it, set as it is made. Nothing is fetched from there.
    property string page

    /// The host of `url`, as www.youtube-nocookie.com.
    function hostOf(url) {
        return String(url).split("/")[2] ?? ""
    }

    /// Opens `url` in the browser when it is a web page; any other kind, as a file or a
    /// program's own scheme, a player has no business opening, so it goes nowhere.
    function openOutside(url) {
        const text = String(url)
        if (text.startsWith("https://") || text.startsWith("http://"))
            Qt.openUrlExternally(url)
    }

    /// `text` safe in an HTML attribute.
    function escaped(text) {
        return String(text).replace(/&/g, "&amp;").replace(/"/g, "&quot;").replace(/</g, "&lt;")
    }

    /// The video the page was made for, so it is made once for each.
    property url loaded

    /// Makes Jot's page for the video, once the card has said which, as the card says so only
    /// after loading the player.
    function load() {
        if (String(root.embed) === "" || root.embed === root.loaded || !view)
            return
        root.loaded = root.embed
        // Its scheme is the player's, as a page only names itself to a player as secure as it.
        root.page = (String(root.embed).startsWith("http:") ? "http" : "https")
                + "://jotwriter.org/"
        view.loadHtml(root.html(), root.page)
    }

    onEmbedChanged: load()

    /// Jot's page: the player filling it, starting at once, as play was just pressed.
    function html() {
        const embed = String(root.embed)
        const source = embed + (embed.includes("?") ? "&" : "?") + "autoplay=1"
        return "<!doctype html><html><head><meta charset=\"utf-8\"><style>"
                + "html,body{margin:0;height:100%;background:#000;overflow:hidden}"
                + "iframe{border:0;width:100%;height:100%}</style></head><body>" + "<iframe src=\""
                + escaped(source) + "\" allow=\"autoplay; encrypted-media; "
                + "fullscreen; picture-in-picture\" allowfullscreen "
                + "referrerpolicy=\"strict-origin-when-cross-origin\"></iframe></body></html>"
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
        backgroundColor: "black"
        settings.playbackRequiresUserGesture: false
        settings.javascriptCanOpenWindows: false
        settings.localContentCanAccessFileUrls: false
        settings.localContentCanAccessRemoteUrls: false
        settings.pluginsEnabled: false
        settings.fullScreenSupportEnabled: true

        Component.onCompleted: root.load()

        onNavigationRequested: request => {
            // Jot's page, and the player's own pages inside it, stay; a link anywhere else
            // opens in the browser, and nothing else goes anywhere.
            const url = String(request.url)
            const ours = request.isMainFrame ? url.startsWith(root.page) || url.startsWith("data:") :
                                               root.hostOf(url) === root.hostOf(root.embed)
            if (ours || url.startsWith("about:")) {
                request.accept()
                return
            }
            request.reject()
            if (request.navigationType === WebEngineNavigationRequest.LinkClickedNavigation)
                root.openOutside(request.url)
        }
        onNewWindowRequested: request => root.openOutside(request.requestedUrl)
        onFullScreenRequested: request => request.accept()
        onPermissionRequested: permission => permission.deny()
    }
}
