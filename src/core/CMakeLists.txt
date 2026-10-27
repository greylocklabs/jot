# Deliberately not a QML module. The CLI links this, and pulling QtQml in would
# cost it startup time for no benefit. QML exposure happens in src/ui.

add_library(jotcore STATIC
    jot/FrontMatter.h
    FrontMatter.cpp
    jot/Fuzzy.h
    Fuzzy.cpp
    jot/MarkdownEdit.h
    MarkdownEdit.cpp
    jot/MonthModel.h
    MonthModel.cpp
    jot/JoplinExport.h
    JoplinExport.cpp
    jot/JoplinImport.h
    JoplinImport.cpp
    jot/Layout.h
    Layout.cpp
    jot/NoteScan.h
    NoteScan.cpp
    jot/Rename.h
    Rename.cpp
    jot/Settings.h
    Settings.cpp
    jot/Vault.h
    Vault.cpp
    jot/VaultWatcher.h
    VaultWatcher.cpp
    jot/VaultIndex.h
    VaultIndex.cpp
    jot/Logging.h
    Logging.cpp
    jot/LogFile.h
    LogFile.cpp
    jot/CodeColors.h
    CodeColors.cpp
    jot/Diagnostics.h
    Diagnostics.cpp
    jot/ProblemReport.h
    ProblemReport.cpp
)


qt_add_resources(jotcore "jot_changelog"
    PREFIX "/jot"
    BASE "${PROJECT_SOURCE_DIR}"
    FILES
        "${PROJECT_SOURCE_DIR}/CHANGELOG.md"
)

# Both are under the SIL Open Font License; the license files sit beside them.
qt_add_resources(jotcore "jot_fonts"
    PREFIX "/jot/fonts"
    BASE "${PROJECT_SOURCE_DIR}/data/fonts"
    FILES
        "${PROJECT_SOURCE_DIR}/data/fonts/Nunito-Variable.ttf"
        "${PROJECT_SOURCE_DIR}/data/fonts/Fraunces-Variable.ttf"
)

target_include_directories(jotcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(jotcore
    PUBLIC
        Qt6::Core
        Qt6::Gui
        greylock::core
        Qt6::Network
        Qt6::Sql
)


# The QML module in src/ui exposes Settings as a foreign type. Its registrar
# can only describe the type if this target publishes its metatypes.
qt_extract_metatypes(jotcore)
