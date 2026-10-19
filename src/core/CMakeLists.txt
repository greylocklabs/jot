# Deliberately not a QML module. The CLI links this, and pulling QtQml in would
# cost it startup time for no benefit. QML exposure happens in src/ui.
find_package(tomlplusplus REQUIRED)

add_library(jotcore STATIC
    jot/FrontMatter.h
    FrontMatter.cpp
    jot/Fuzzy.h
    Fuzzy.cpp
    jot/MarkdownEdit.h
    MarkdownEdit.cpp
    jot/MonthModel.h
    MonthModel.cpp
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
    jot/Identity.h
    Identity.cpp
    jot/Logging.h
    Logging.cpp
    jot/LogFile.h
    LogFile.cpp
    jot/Color.h
    Color.cpp
    jot/CodeColors.h
    CodeColors.cpp
    jot/Base16.h
    Base16.cpp
    jot/ThemeSpec.h
    jot/ThemeLoader.h
    ThemeLoader.cpp
    jot/ThemeLibrary.h
    ThemeLibrary.cpp
    jot/Releases.h
    Releases.cpp
    jot/ReleaseNotes.h
    ReleaseNotes.cpp
    jot/UpdateCheck.h
    UpdateCheck.cpp
    jot/Diagnostics.h
    Diagnostics.cpp
    jot/ProblemReport.h
    ProblemReport.cpp
)

# Built-in themes are compiled in, so the app works uninstalled and a broken
# install can never leave it without its default look. They are the suite's
# themes, from core, so a theme file works in any app; they keep their
# :/jot/themes/ paths.
qt_add_resources(jotcore "jot_themes"
    PREFIX "/jot"
    BASE "${PROJECT_SOURCE_DIR}/core"
    FILES
        "${PROJECT_SOURCE_DIR}/core/themes/callie.toml"
        "${PROJECT_SOURCE_DIR}/core/themes/callie-light.toml"
        "${PROJECT_SOURCE_DIR}/core/themes/minimal.toml"
        "${PROJECT_SOURCE_DIR}/core/themes/minimal-dark.toml"
        "${PROJECT_SOURCE_DIR}/core/themes/paper.toml"
        "${PROJECT_SOURCE_DIR}/core/themes/tomorrow.toml"
        "${PROJECT_SOURCE_DIR}/core/themes/tomorrow-night.toml"
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
        Qt6::Network
        Qt6::Sql
    PRIVATE
        tomlplusplus::tomlplusplus
)

target_compile_definitions(jotcore PRIVATE
    JOT_APP_ID="${JOT_APP_ID}"
    JOT_DEVEL_BUILD=$<BOOL:${JOT_DEVEL}>
)

# The QML module in src/ui exposes Settings as a foreign type. Its registrar
# can only describe the type if this target publishes its metatypes.
qt_extract_metatypes(jotcore)
