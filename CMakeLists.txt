cmake_minimum_required(VERSION 3.28)

project(jot
    VERSION 0.0.10
    DESCRIPTION "Plain-file notes for Linux"
    HOMEPAGE_URL "https://jotwriter.org"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

set(JOT_APP_ID "org.callieapp.Jot" CACHE STRING "Reverse-DNS application id")
# A development build runs beside an installed Jot, with its own app id,
# folders and keyring entries, so the two never share a vault index or settings.
option(JOT_DEVEL "Give this build its own identity, apart from an installed Jot" OFF)

option(JOT_BUILD_TESTS "Build the unit tests" ON)
option(JOT_WERROR "Treat compiler warnings as errors" OFF)
# Builds in the source tree's path, so it is for development builds only.
option(JOT_LIVE_QML "Add --live-qml to the app and gallery to reload QML from src/" OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(JOT_WERROR)
        add_compile_options(-Werror)
    endif()
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 Sql)
qt_standard_project_setup(REQUIRES 6.8)

# The built-in themes come from the core submodule, which a clone made without
# --recurse-submodules leaves empty.
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/core/themes/callie.toml")
    message(FATAL_ERROR "core/ is not checked out; run git submodule update --init")
endif()

if(JOT_BUILD_TESTS)
    enable_testing()
endif()

# Shared code and the built-in themes, from greylocklabs/core, built for Jot.
set(GREYLOCK_APP_ID "${JOT_APP_ID}")
set(GREYLOCK_APP_NAME "Jot")
set(GREYLOCK_APP_DIR "jot")
set(GREYLOCK_APP_ENV "JOT")
set(GREYLOCK_APP_REPO "greylocklabs/jot")
set(GREYLOCK_APP_DEVEL ${JOT_DEVEL})
add_subdirectory(core)
add_subdirectory(src/core)
add_subdirectory(src/ui)
add_subdirectory(src/gui)
add_subdirectory(src/cli)
add_subdirectory(src/gallery)
add_subdirectory(data)

# Built-in themes are compiled in; installed copies are a starting point for
# people writing their own.
install(DIRECTORY core/themes/ DESTINATION ${CMAKE_INSTALL_DATADIR}/jot/themes
    FILES_MATCHING PATTERN "*.toml")

if(JOT_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Formatting is driven by one script so that CMake, CI and a local run agree.
# The script comes from the core submodule, which release tarballs leave out.
if(EXISTS "${CMAKE_SOURCE_DIR}/core/tooling/format.sh")
    add_custom_target(format
        COMMAND "${CMAKE_SOURCE_DIR}/core/tooling/format.sh" fix
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Formatting C++ and QML sources"
        VERBATIM
    )

    add_custom_target(format-check
        COMMAND "${CMAKE_SOURCE_DIR}/core/tooling/format.sh" check
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Checking C++ and QML formatting"
        VERBATIM
    )
endif()
