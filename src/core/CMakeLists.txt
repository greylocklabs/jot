# Deliberately not a QML module. The CLI links this, and pulling QtQml in would
# cost it startup time for no benefit. QML exposure happens in src/ui.

# The app key Dropbox knows Jot by, for syncing with it. An explicit value wins, then the
# JOT_DROPBOX_APP_KEY environment variable, as the release workflow sets it. Without one, Dropbox
# is left out of the build.
set(JOT_DROPBOX_APP_KEY "" CACHE STRING "Dropbox app key")
set(jot_dropbox_app_key "${JOT_DROPBOX_APP_KEY}")
if(jot_dropbox_app_key STREQUAL "" AND NOT "$ENV{JOT_DROPBOX_APP_KEY}" STREQUAL "")
    set(jot_dropbox_app_key "$ENV{JOT_DROPBOX_APP_KEY}")
endif()
# Pasted into a C++ string literal, so anything but what Dropbox's keys hold is refused.
if(NOT jot_dropbox_app_key MATCHES "^[A-Za-z0-9]*$")
    message(FATAL_ERROR "The Dropbox app key holds unexpected characters")
endif()

# Google's OAuth client for Jot, for syncing with Google Drive, found as the Dropbox key is.
set(JOT_GOOGLE_CLIENT_ID "" CACHE STRING "Google OAuth client id")
set(JOT_GOOGLE_CLIENT_SECRET "" CACHE STRING "Google OAuth client secret")
set(jot_google_client_id "${JOT_GOOGLE_CLIENT_ID}")
set(jot_google_client_secret "${JOT_GOOGLE_CLIENT_SECRET}")
if(jot_google_client_id STREQUAL "" AND NOT "$ENV{JOT_GOOGLE_CLIENT_ID}" STREQUAL "")
    set(jot_google_client_id "$ENV{JOT_GOOGLE_CLIENT_ID}")
    set(jot_google_client_secret "$ENV{JOT_GOOGLE_CLIENT_SECRET}")
endif()

foreach(value IN ITEMS "${jot_google_client_id}" "${jot_google_client_secret}")
    if(NOT value MATCHES "^[A-Za-z0-9._-]*$")
        message(FATAL_ERROR "The Google OAuth client id or secret holds unexpected characters")
    endif()
endforeach()
# In a header made here, not on the compiler's command line, which build logs print.
configure_file(SyncBuildConfig.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/jot/SyncBuildConfig.h"
               @ONLY)

add_library(jotcore STATIC
    jot/DataCheck.h
    DataCheck.cpp
    jot/DataTable.h
    DataTable.cpp
    jot/FrontMatter.h
    FrontMatter.cpp
    jot/Fuzzy.h
    Fuzzy.cpp
    jot/MarkdownEdit.h
    MarkdownEdit.cpp
    jot/Merge.h
    Merge.cpp
    jot/MonthModel.h
    MonthModel.cpp
    jot/JoplinExport.h
    JoplinExport.cpp
    JoplinDatabase.cpp
    jot/JoplinPatch.h
    JoplinPatch.cpp
    jot/JoplinImport.h
    JoplinImport.cpp
    jot/Layout.h
    Layout.cpp
    jot/NoteHistory.h
    NoteHistory.cpp
    jot/NoteScan.h
    NoteScan.cpp
    jot/NoteTrail.h
    NoteTrail.cpp
    jot/Conflicts.h
    Conflicts.cpp
    jot/FolderTransport.h
    FolderTransport.cpp
    jot/Secrets.h
    Secrets.cpp
    jot/SigV4.h
    SigV4.cpp
    jot/Sync.h
    Sync.cpp
    jot/S3Transport.h
    S3Transport.cpp
    jot/DropboxTransport.h
    DropboxTransport.cpp
    jot/DriveTransport.h
    DriveTransport.cpp
    jot/GitSync.h
    GitSync.cpp
    SyncRemotes.cpp
    jot/TaskLine.h
    TaskLine.cpp
    jot/ToDos.h
    ToDos.cpp
    jot/QuickAdd.h
    QuickAdd.cpp
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
    PRIVATE
        Qt6Keychain::Qt6Keychain
        PkgConfig::YAML
)
target_include_directories(jotcore PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")


# The QML module in src/ui exposes Settings as a foreign type. Its registrar
# can only describe the type if this target publishes its metatypes.
qt_extract_metatypes(jotcore)
