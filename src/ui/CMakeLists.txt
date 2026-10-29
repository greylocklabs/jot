# The QML files sit at this module's root, which is what Qt expects. Keeping
# them in a subdirectory would require a hand-written qmldir (policy QTP0004).
qt_add_qml_module(jotui
    URI Jot.Ui
    VERSION 1.0
    STATIC
    # qmllint and qmlls expect the output path to mirror the module URI.
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Jot/Ui
    # Theme exposes QColor properties; older qmllint only maps them to QML's
    # color type when QtQuick is a declared dependency.
    DEPENDENCIES QtQuick
    SOURCES
        ThemeController.h
        ThemeController.cpp
        WindowButtons.h
        WindowButtons.cpp
        LiveQml.h
        LiveQml.cpp
        BacklinksModel.h
        BacklinksModel.cpp
        CodeHighlighter.h
        CodeHighlighter.cpp
        EmojiList.h
        EmojiList.cpp
        MarkdownHighlighter.h
        MarkdownHighlighter.cpp
        MarkdownStyler.h
        MarkdownStyler.cpp
        NotebookModel.h
        NotebookModel.cpp
        NoteListModel.h
        NoteListModel.cpp
        NoteReader.h
        NoteReader.cpp
        SwitcherModel.h
        SwitcherModel.cpp
        TagModel.h
        TagModel.cpp
        TaskModel.h
        TaskModel.cpp
        ToDoReminders.h
        ToDoReminders.cpp
        VaultController.h
        VaultController.cpp
        ReleaseInfo.h
        ReleaseInfo.cpp
        SettingsForeign.h
        StartAtLoginSetting.h
        MonthModelForeign.h
        SupportActions.h
        SupportActions.cpp
        ThemesController.h
        ThemesController.cpp
        KeyRouter.h
        KeyRouter.cpp
        ViEditor.h
        ViEditor.cpp
    QML_FILES
        Main.qml
        AboutDialog.qml
        Backlinks.qml
        CheckBox.qml
        CloseButton.qml
        ColumnSplitter.qml
        CommandPalette.qml
        DangerButton.qml
        Dropdown.qml
        EmojiPicker.qml
        DatePicker.qml
        Field.qml
        FormatBar.qml
        FormRow.qml
        Glyph.qml
        KeyCap.qml
        KeyHelp.qml
        LazyPopup.qml
        LinkPreview.qml
        Logo.qml
        MenuCard.qml
        MenuEntry.qml
        ModeSwitch.qml
        NoteEditor.qml
        NoteList.qml
        PickerButton.qml
        PillButton.qml
        QuickAdd.qml
        QuickSwitcher.qml
        ResizeFrame.qml
        SettingsDialog.qml
        Scrim.qml
        Sidebar.qml
        SoftShadow.qml
        StickerButton.qml
        SuggestionPopup.qml
        SwitcherRow.qml
        ThemeEditor.qml
        WhichKey.qml
        Tip.qml
        TaskDetails.qml
        ReportDialog.qml
        TrashConfirm.qml
        JoplinImport.qml
        TaskList.qml
        Toggle.qml
        UpdateDialog.qml
        Welcome.qml
        WhatsNewDialog.qml
        HistoryDialog.qml
        WindowControls.qml
)

qt_add_resources(jotui "jot_logo"
    PREFIX "/jot"
    BASE "${PROJECT_SOURCE_DIR}"
    FILES "${PROJECT_SOURCE_DIR}/assets/logo.png"
)

# Names for the emoji picker's search, made by data/emoji.pl.
qt_add_resources(jotui "jot_emoji"
    PREFIX "/jot"
    BASE "${PROJECT_SOURCE_DIR}/data"
    FILES "${PROJECT_SOURCE_DIR}/data/emoji.txt"
)

target_include_directories(jotui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(jotui PUBLIC Qt6::Quick Qt6::QuickControls2 jotcore)
target_link_libraries(jotui PRIVATE KF6::SyntaxHighlighting)
