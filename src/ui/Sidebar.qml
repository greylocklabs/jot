pragma ComponentBehavior: Bound
import Jot.Ui
import QtQuick
import QtQuick.Controls

/// The notebooks and the tags below them, each as a tree that folds. Choosing either fills
/// the note list. Notes and notebooks dropped on a notebook move into it; a notebook is
/// renamed by double-clicking it.
Rectangle {
    id: root

    /// The notebook whose name is being typed, or "".
    property string renamingPath

    /// A note was made from a notebook's menu, and is shown; its title should take the typing.
    signal noteMade
    /// Import from Joplin was chosen for the notebook at `folder`.
    signal joplinImportRequested(string folder)
    signal todoistImportRequested
    /// A notebook, tag, Tasks or Trash opened from the keyboard, so its list takes the keys.
    signal entered

    /// Makes a notebook at the top of the vault, ready to name.
    function newNotebook() {
        root.renamingPath = Vault.newNotebook()
    }

    /// The rows the keyboard moves through, top to bottom: Tasks, Favorites, notebooks, tags and
    /// Trash.
    function rows() {
        const found = []
        const walk = item => {
            for (const child of item.children) {
                if (child.isSidebarRow === true && child.visible)
                    found.push(child)
                else
                    walk(child)
            }
        }
        walk(root)
        // The trash sits at the foot, below everything that scrolls.
        const trash = found.indexOf(trashEntry)
        if (trash >= 0)
            found.push(found.splice(trash, 1)[0])
        return found
    }

    /// Whether the keyboard is on one of the sidebar's rows.
    function hasKeys() {
        return rows().some(row => row.activeFocus)
    }

    /// Puts the keyboard on the row shown, or the first.
    function focusRows() {
        const all = rows()
        const shown = all.find(row => row.selected) ?? all[0]
        if (shown)
            shown.forceActiveFocus()
    }

    /// Moves the keyboard to the next, previous, first or last row, by KeyRouter's action id.
    function step(id) {
        const all = rows()
        if (all.length === 0)
            return
        let at = all.findIndex(row => row.activeFocus)
        if (id === "firstNote" || at < 0)
            at = 0
        else if (id === "lastNote")
            at = all.length - 1
        else if (id === "nextNote")
            at = Math.min(at + 1, all.length - 1)
        else
            at = Math.max(at - 1, 0)
        all[at].forceActiveFocus()
    }

    /// Moves what was dropped, a note or a notebook, into the notebook at `path`.
    function dropInto(path, drop) {
        if (drop.formats.includes("application/x-jot-note")) {
            Vault.moveNoteTo(drop.getDataAsString("application/x-jot-note"), path)
        } else if (drop.formats.includes("application/x-jot-notebook")) {
            Vault.moveNotebook(drop.getDataAsString("application/x-jot-notebook"), path)
        }
        drop.accept(Qt.MoveAction)
    }

    color: Theme.surface

    KeyRouter {
        id: shortcuts
    }

    component Heading: Text {
        leftPadding: Theme.space5
        topPadding: Theme.space5
        bottomPadding: Theme.space2
        color: Theme.textFaint
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textXs
        font.weight: Theme.weightBold
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 0.6
    }

    /// A row's emoji and name, the emoji drawn on its own at Theme.emojiSize.
    component RowLabel: Item {
        id: rowLabel

        property string emoji
        property string text
        property bool selected
        /// Where the name starts, past the emoji and the room after it.
        readonly property real nameX: rowName.x

        implicitWidth: rowName.x + rowName.implicitWidth
        implicitHeight: Math.max(rowEmoji.implicitHeight, rowName.implicitHeight)

        Text {
            id: rowEmoji

            anchors.verticalCenter: parent.verticalCenter
            width: visible ? Theme.emojiBox : 0
            visible: rowLabel.emoji !== ""
            text: rowLabel.emoji
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: Theme.emojiSize
        }
        Text {
            id: rowName

            anchors {
                left: rowEmoji.right
                leftMargin: rowEmoji.visible ? Theme.space2 : 0
                right: parent.right
                verticalCenter: parent.verticalCenter
            }
            text: rowLabel.text
            elide: Text.ElideRight
            color: rowLabel.selected ? Theme.text : Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textMd
            font.weight: rowLabel.selected ? Theme.weightBold : Theme.weightStrong
        }
    }

    component Entry: AbstractButton {
        id: row

        property bool selected: false
        property string label
        readonly property bool isSidebarRow: true

        height: Theme.listRowHeight
        hoverEnabled: true
        Accessible.name: label
        // Enter, or l in vi mode, opens the row and gives its list the keyboard.
        Keys.onReturnPressed: {
            row.clicked()
            root.entered()
        }
        Keys.onEnterPressed: {
            row.clicked()
            root.entered()
        }
        Keys.onPressed: event => {
            if (Settings.viMode && event.text === "l" && event.modifiers === Qt.NoModifier) {
                row.clicked()
                root.entered()
                event.accepted = true
            }
        }

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }
        background: Rectangle {
            anchors.fill: parent
            anchors.leftMargin: Theme.space3
            anchors.rightMargin: Theme.space3
            radius: Theme.radiusSm
            color: row.selected ? Theme.tint(Theme.accent, 0.18) : row.hovered ? Theme.surfaceAlt :
                                                                                 "transparent"

            border.width: row.activeFocus ? 1 : 0
            border.color: Theme.accent
        }
    }

    ScrollView {
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            bottom: trashEntry.top
        }
        contentWidth: availableWidth

        Column {
            width: parent.width
            // Room between the title bar and the first entry.
            topPadding: Theme.space3

            Entry {
                id: tasks

                width: root.width
                label: qsTr("Tasks")
                selected: Vault.showingTasks && Vault.tasks.tag === ""
                onClicked: {
                    Vault.showingTasks = true
                    Vault.tasks.tag = ""
                }

                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: eventPoint => {
                        viewMenu.view = "tasks"
                        viewMenu.popup(tasks, eventPoint.position.x, eventPoint.position.y)
                    }
                }

                contentItem: Item {
                    RowLabel {
                        anchors {
                            left: parent.left
                            // In line with the notebooks' names, past where their folds go.
                            leftMargin: Theme.space4 + Theme.smallGlyphSize + Theme.space3
                            verticalCenter: parent.verticalCenter
                        }
                        emoji: Vault.viewEmoji.tasks ?? ""
                        text: tasks.label
                        selected: tasks.selected
                    }
                    Text {
                        anchors {
                            right: parent.right
                            rightMargin: Theme.space5
                            verticalCenter: parent.verticalCenter
                        }
                        // What is due today or overdue, as the badge on a to-do app does.
                        visible: Vault.tasks.todayCount > 0
                        text: Vault.tasks.todayCount
                        color: Theme.accent
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textXs
                        font.weight: Theme.weightBold
                    }
                }
            }

            // The favorite notes, listed like a notebook's in the order given.
            Entry {
                id: favorites

                objectName: "favorites"
                width: root.width
                label: qsTr("Favorites")
                selected: !Vault.showingTasks && !Vault.showingTrash && Vault.shownTag === ""
                          && Vault.notes.favorites
                onClicked: {
                    Vault.dropTag()
                    Vault.showingTasks = false
                    Vault.showingTrash = false
                    Vault.notes.favorites = true
                }

                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: eventPoint => {
                        viewMenu.view = "favorites"
                        viewMenu.popup(favorites, eventPoint.position.x, eventPoint.position.y)
                    }
                }
                // A note dropped here becomes a favorite.
                DropArea {
                    id: dropFavorite
                    anchors.fill: parent
                    keys: ["application/x-jot-note"]
                    enabled: Vault.writable
                    onDropped: drop => {
                        Vault.setFavorite(drop.getDataAsString("application/x-jot-note"), true)
                        drop.accept(Qt.CopyAction)
                    }
                }

                contentItem: Item {
                    Rectangle {
                        anchors {
                            fill: parent
                            leftMargin: Theme.space3
                            rightMargin: Theme.space3
                        }
                        visible: dropFavorite.containsDrag
                        radius: Theme.radiusSm
                        color: "transparent"
                        border.color: Theme.accent
                        border.width: 2
                    }
                    RowLabel {
                        anchors {
                            left: parent.left
                            leftMargin: Theme.space4 + Theme.smallGlyphSize + Theme.space3
                            verticalCenter: parent.verticalCenter
                        }
                        emoji: Vault.viewEmoji.favorites ?? ""
                        text: favorites.label
                        selected: favorites.selected
                    }
                    Text {
                        anchors {
                            right: parent.right
                            rightMargin: Theme.space5
                            verticalCenter: parent.verticalCenter
                        }
                        visible: Vault.favorites.length > 0
                        text: Vault.favorites.length
                        color: Theme.textFaint
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textXs
                        font.weight: Theme.weightBold
                    }
                }
            }

            Item {
                width: root.width
                height: notebooksHeading.height

                Heading {
                    id: notebooksHeading
                    text: qsTr("Notebooks")
                }
                AbstractButton {
                    id: addNotebook

                    objectName: "addNotebook"

                    anchors {
                        right: parent.right
                        rightMargin: Theme.space4
                        bottom: parent.bottom
                        bottomMargin: Theme.space1
                    }
                    width: Theme.listRowHeight - Theme.space2
                    height: width
                    visible: Vault.writable
                    focusPolicy: Qt.NoFocus
                    Accessible.name: qsTr("New notebook")
                    onClicked: root.newNotebook()

                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                    }
                    background: Rectangle {
                        radius: Theme.radiusSm
                        color: addNotebook.hovered ? Theme.surfaceAlt : Theme.tint(Theme.surfaceAlt,
                                                                                   0)
                    }
                    contentItem: Item {
                        Glyph {
                            anchors.centerIn: parent
                            width: Theme.smallGlyphSize
                            height: Theme.smallGlyphSize
                            name: "plus"
                            color: addNotebook.hovered ? Theme.text : Theme.textFaint
                            stroke: Theme.smallGlyphStroke
                        }
                    }
                    Tip {
                        visible: addNotebook.hovered
                        text: {
                            shortcuts.actions
                            const key = shortcuts.keyText("newNotebook")
                            return key === "" ? addNotebook.Accessible.name : qsTr("%1 (%2)").arg(
                                                    addNotebook.Accessible.name).arg(key)
                        }
                        delay: Theme.durSlow * 2
                    }
                }
            }

            Repeater {
                model: Vault.notebooks

                Entry {
                    id: notebook

                    required property string path
                    required property string name
                    required property int depth
                    required property bool hasChildren
                    required property bool expanded
                    required property int count

                    readonly property string emoji: Vault.notebookEmoji[path] ?? ""
                    readonly property bool renaming: root.renamingPath !== "" && root.renamingPath
                                                     === path

                    width: root.width
                    label: name
                    selected: !Vault.showingTasks && !Vault.showingTrash && Vault.shownTag === ""
                              && Vault.notes.tag === "" && !Vault.notes.favorites
                              && Vault.notes.notebook === path
                    onClicked: {
                        Vault.dropTag()
                        Vault.showingTasks = false
                        Vault.showingTrash = false
                        Vault.notes.notebook = path
                    }
                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        // At the click, which Wayland does not share as a cursor position.
                        onTapped: eventPoint => {
                            notebookMenu.path = notebook.path
                            notebookMenu.templates = Vault.templates()
                            notebookMenu.popup(notebook, eventPoint.position.x,
                                               eventPoint.position.y)
                        }
                    }

                    // Delete on the notebook clicked asks before it goes to the trash.
                    Keys.onDeletePressed: event => {
                        if (notebook.path !== "" && Vault.writable && !notebook.renaming) {
                            trashConfirm.askForNotebook(notebook.path)
                            event.accepted = true
                        }
                    }

                    // "All notes" is the vault itself, which has no name to change.
                    onDoubleClicked: {
                        if (path !== "" && Vault.writable)
                            root.renamingPath = path
                    }

                    // A notebook drags onto another to go inside it.
                    // It floats under the pointer as a picture of the row, as notes do.
                    DragHandler {
                        id: notebookDrag
                        target: null
                        enabled: notebook.path !== "" && Vault.writable && !notebook.renaming
                        onActiveChanged: {
                            if (!active) {
                                notebook.Drag.active = false
                                return
                            }
                            const grabbed = centroid.pressPosition
                            notebook.grabToImage(result => {
                                if (!notebookDrag.active)
                                    return
                                notebook.Drag.imageSource = result.url
                                notebook.Drag.hotSpot = grabbed
                                notebook.Drag.active = true
                            })
                        }
                    }
                    Drag.dragType: Drag.Automatic
                    Drag.supportedActions: Qt.MoveAction
                    Drag.mimeData: ({
                                        "application/x-jot-notebook": notebook.path
                                    })

                    DropArea {
                        id: dropHere
                        anchors.fill: parent
                        keys: ["application/x-jot-note", "application/x-jot-notebook"]
                        enabled: Vault.writable
                        onDropped: drop => root.dropInto(notebook.path, drop)
                    }

                    Rectangle {
                        anchors {
                            fill: parent
                            leftMargin: Theme.space3
                            rightMargin: Theme.space3
                        }
                        visible: dropHere.containsDrag
                        radius: Theme.radiusSm
                        color: "transparent"
                        border.width: 2
                        border.color: Theme.accent
                    }

                    // Typing a new name, in the row's place and shape, its text where the name
                    // was: Return or leaving keeps it, Escape does not.
                    Loader {
                        anchors {
                            fill: parent
                            // Where the name is drawn, past its emoji, less the field's own padding.
                            leftMargin: notebook.contentItem.x + nameLabel.x + nameLabel.nameX
                                        - Theme.space2
                            rightMargin: Theme.space3
                            topMargin: Theme.space1
                            bottomMargin: Theme.space1
                        }
                        active: notebook.renaming
                        sourceComponent: Field {
                            id: nameField

                            property bool done: false

                            function finish(keep) {
                                if (done)
                                    return
                                done = true
                                if (keep && text.trim() !== "" && text !== notebook.name)
                                    Vault.renameNotebook(notebook.path, text)
                                root.renamingPath = ""
                            }

                            objectName: "notebookName"
                            text: notebook.name
                            topPadding: 0
                            bottomPadding: 0
                            leftPadding: Theme.space2
                            font.pixelSize: Theme.textMd
                            background: Rectangle {
                                radius: Theme.radiusSm
                                color: Theme.surface
                                border.width: 1
                                border.color: Theme.accent
                            }
                            Accessible.name: qsTr("Name of %1").arg(notebook.name)
                            Component.onCompleted: {
                                forceActiveFocus()
                                selectAll()
                            }
                            onAccepted: finish(true)
                            onActiveFocusChanged: {
                                if (!activeFocus)
                                    finish(true)
                            }
                            Keys.onEscapePressed: finish(false)
                        }
                    }

                    contentItem: Item {
                        AbstractButton {
                            id: fold

                            x: Theme.space4 + notebook.depth * Theme.treeIndent
                            anchors.verticalCenter: parent.verticalCenter
                            width: Theme.smallGlyphSize + Theme.space3
                            height: parent.height
                            visible: notebook.hasChildren
                            Accessible.name: notebook.expanded ? qsTr("Fold %1").arg(notebook.name) :
                                                                 qsTr("Open %1").arg(notebook.name)
                            onClicked: Vault.notebooks.toggle(notebook.path)

                            contentItem: Glyph {
                                name: notebook.expanded ? "chevron-down" : "chevron-right"
                                color: Theme.textFaint
                                stroke: Theme.smallGlyphStroke
                                implicitWidth: Theme.smallGlyphSize
                                implicitHeight: Theme.smallGlyphSize
                            }
                        }
                        RowLabel {
                            id: nameLabel

                            anchors {
                                left: fold.right
                                right: countLabel.left
                                rightMargin: Theme.space3
                                verticalCenter: parent.verticalCenter
                            }
                            emoji: notebook.emoji
                            // Renaming, only the emoji stays, beside the name being typed.
                            text: notebook.renaming ? "" : notebook.name
                            selected: notebook.selected
                        }
                        Text {
                            id: countLabel

                            anchors {
                                right: parent.right
                                rightMargin: Theme.space5
                                verticalCenter: parent.verticalCenter
                            }
                            text: notebook.count
                            color: Theme.textFaint
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textXs
                            font.features: {
                                "tnum": 1
                            }
                        }
                    }
                }
            }

            Heading {
                visible: Vault.tagTree.count > 0
                text: qsTr("Tags")
            }

            // Nested tags fold as notebooks do.
            Repeater {
                model: Vault.tagTree

                Entry {
                    id: tagRow

                    required property string tag
                    required property string name
                    required property int depth
                    required property bool hasChildren
                    required property bool expanded
                    required property int count

                    width: root.width
                    label: "#" + tag
                    // In Tasks, a tag narrows the to-dos; elsewhere it lists its notes.
                    // A tag shows its notes and to-dos together; again, it goes back.
                    selected: Vault.shownTag === tag
                    onClicked: Vault.shownTag = Vault.shownTag === tag ? "" : tag

                    contentItem: Item {
                        AbstractButton {
                            id: tagFold

                            x: Theme.space4 + tagRow.depth * Theme.treeIndent
                            anchors.verticalCenter: parent.verticalCenter
                            width: Theme.smallGlyphSize + Theme.space3
                            height: parent.height
                            visible: tagRow.hasChildren
                            Accessible.name: tagRow.expanded ? qsTr("Fold #%1").arg(tagRow.tag) :
                                                               qsTr("Open #%1").arg(tagRow.tag)
                            onClicked: Vault.tagTree.toggle(tagRow.tag)

                            contentItem: Glyph {
                                name: tagRow.expanded ? "chevron-down" : "chevron-right"
                                color: Theme.textFaint
                                stroke: Theme.smallGlyphStroke
                                implicitWidth: Theme.smallGlyphSize
                                implicitHeight: Theme.smallGlyphSize
                            }
                        }
                        Text {
                            anchors {
                                left: tagFold.right
                                right: tagCount.left
                                rightMargin: Theme.space3
                                verticalCenter: parent.verticalCenter
                            }
                            // A nested tag shows its last part, under the tag it is in.
                            text: "#" + tagRow.name
                            elide: Text.ElideRight
                            color: tagRow.selected ? Theme.text : Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textMd
                            font.weight: tagRow.selected ? Theme.weightBold : Theme.weightStrong
                        }
                        Text {
                            id: tagCount

                            anchors {
                                right: parent.right
                                rightMargin: Theme.space5
                                verticalCenter: parent.verticalCenter
                            }
                            text: tagRow.count
                            color: Theme.textFaint
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textXs
                            font.features: {
                                "tnum": 1
                            }
                        }
                    }
                }
            }
        }
    }

    // At the foot, below everything that scrolls.
    Entry {
        id: trashEntry

        objectName: "trashEntry"
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            bottomMargin: Theme.space3
        }
        label: qsTr("Trash")
        selected: Vault.showingTrash
        onClicked: Vault.showingTrash = true

        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: eventPoint => {
                viewMenu.view = "trash"
                viewMenu.popup(trashEntry, eventPoint.position.x, eventPoint.position.y)
            }
        }

        contentItem: Item {
            RowLabel {
                anchors {
                    left: parent.left
                    // In line with the notebooks' names, past where their folds go.
                    leftMargin: Theme.space4 + Theme.smallGlyphSize + Theme.space3
                    verticalCenter: parent.verticalCenter
                }
                emoji: Vault.viewEmoji.trash ?? ""
                text: trashEntry.label
                selected: trashEntry.selected
            }
        }
    }

    // For Tasks and Trash, which are no notebooks.
    MenuCard {
        id: viewMenu

        property string view

        MenuEntry {
            text: qsTr("Choose an emoji...")
            enabled: Vault.writable
            onTriggered: emojiPicker.chooseForView(viewMenu.view)
        }
    }

    Rectangle {
        anchors {
            top: parent.top
            bottom: parent.bottom
            right: parent.right
        }
        width: 1
        color: Theme.hairline
    }

    MenuCard {
        id: notebookMenu

        /// The notebook it is for; "" is All notes, the vault itself.
        property string path
        /// The vault's templates, read as it opens.
        property var templates: []

        MenuEntry {
            text: qsTr("New note")
            enabled: Vault.writable
            onTriggered: {
                Vault.showingTasks = false
                Vault.showingTrash = false
                Vault.dropTag()
                Vault.notes.tag = ""
                Vault.notes.notebook = notebookMenu.path
                if (Vault.newNote() !== "")
                    root.noteMade()
            }
        }
        // A note made from one of the vault's templates, in this notebook.
        MenuCard {
            title: qsTr("New from template")
            enabled: Vault.writable && notebookMenu.templates.length > 0
            minimumWidth: 160

            Repeater {
                model: notebookMenu.templates

                MenuEntry {
                    required property var modelData
                    text: modelData.title
                    onTriggered: {
                        Vault.showingTasks = false
                        Vault.showingTrash = false
                        Vault.dropTag()
                        Vault.notes.tag = ""
                        Vault.notes.notebook = notebookMenu.path
                        if (Vault.newNoteFromTemplate(modelData.path) !== "")
                            root.noteMade()
                    }
                }
            }
        }
        // A data file that opens as a table, in this notebook.
        MenuCard {
            title: qsTr("New file")
            enabled: Vault.writable
            minimumWidth: 160

            Repeater {
                model: Vault.dataKinds

                MenuEntry {
                    required property var modelData
                    text: modelData.label
                    onTriggered: {
                        Vault.showingTasks = false
                        Vault.showingTrash = false
                        Vault.dropTag()
                        Vault.notes.tag = ""
                        Vault.notes.notebook = notebookMenu.path
                        if (Vault.newFile(modelData.kind) !== "")
                            root.noteMade()
                    }
                }
            }
        }
        MenuEntry {
            text: notebookMenu.path === "" ? qsTr("New notebook") : qsTr("New notebook inside")
            enabled: Vault.writable
            onTriggered: root.renamingPath = Vault.newNotebook(notebookMenu.path)
        }
        MenuEntry {
            text: qsTr("Rename")
            visible: notebookMenu.path !== ""
            height: visible ? implicitHeight : 0
            enabled: Vault.writable
            onTriggered: root.renamingPath = notebookMenu.path
        }
        MenuEntry {
            text: qsTr("Choose an emoji...")
            visible: notebookMenu.path !== ""
            height: visible ? implicitHeight : 0
            enabled: Vault.writable
            onTriggered: emojiPicker.choose(notebookMenu.path)
        }
        MenuEntry {
            text: qsTr("Word target...")
            visible: notebookMenu.path !== ""
            height: visible ? implicitHeight : 0
            enabled: Vault.writable
            onTriggered: notebookTarget.openFor(notebookMenu.path)
        }
        MenuEntry {
            text: notebookMenu.path === "" ? qsTr("Import from Joplin...") : qsTr(
                                                 "Import from Joplin into here...")
            enabled: Vault.writable
            onTriggered: root.joplinImportRequested(notebookMenu.path)
        }
        MenuEntry {
            // Todoist's projects always go to Task Lists, so only from the top.
            visible: notebookMenu.path === ""
            height: visible ? implicitHeight : 0
            text: qsTr("Import from Todoist...")
            enabled: Vault.writable
            onTriggered: root.todoistImportRequested()
        }
        MenuEntry {
            text: qsTr("Show in Files")
            onTriggered: Vault.showInFiles(notebookMenu.path)
        }
        MenuEntry {
            objectName: "hideNotebook"
            text: qsTr("Hide this notebook")
            visible: notebookMenu.path !== ""
            height: visible ? implicitHeight : 0
            enabled: Vault.writable
            onTriggered: Vault.setHidden(notebookMenu.path, true)
        }
        MenuEntry {
            objectName: "ignoreNotebook"
            text: qsTr("Ignore this notebook")
            visible: notebookMenu.path !== ""
            height: visible ? implicitHeight : 0
            enabled: Vault.writable
            onTriggered: Vault.ignoreNotebook(notebookMenu.path)
        }
        MenuEntry {
            text: qsTr("Move to trash...")
            visible: notebookMenu.path !== ""
            height: visible ? implicitHeight : 0
            enabled: Vault.writable
            onTriggered: trashConfirm.askForNotebook(notebookMenu.path)
        }
    }

    EmojiPicker {
        id: emojiPicker
    }

    NotebookTarget {
        id: notebookTarget
    }

    TrashConfirm {
        id: trashConfirm
    }
}
