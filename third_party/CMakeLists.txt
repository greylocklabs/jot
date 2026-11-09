# MicroTeX draws LaTeX math with Qt. Built here from its sources, as its own build makes
# sample programs and needs Qt Widgets; only the library and its Qt drawing are wanted.
set(MICROTEX "${CMAKE_CURRENT_SOURCE_DIR}/microtex")
find_package(PkgConfig REQUIRED)
pkg_check_modules(tinyxml2 REQUIRED IMPORTED_TARGET tinyxml2)

file(GLOB_RECURSE microtex_sources CONFIGURE_DEPENDS
    "${MICROTEX}/src/atom/*.cpp" "${MICROTEX}/src/box/*.cpp" "${MICROTEX}/src/core/*.cpp"
    "${MICROTEX}/src/fonts/*.cpp" "${MICROTEX}/src/utils/*.cpp" "${MICROTEX}/src/res/*.cpp")
add_library(microtex STATIC
    ${microtex_sources}
    "${MICROTEX}/src/latex.cpp"
    "${MICROTEX}/src/render.cpp"
    "${MICROTEX}/src/platform/qt/graphic_qt.cpp")
target_include_directories(microtex SYSTEM PUBLIC "${MICROTEX}/src")
target_compile_definitions(microtex PUBLIC BUILD_QT)
target_link_libraries(microtex PRIVATE Qt6::Gui PkgConfig::tinyxml2)
# Not Jot's code, so not held to its warnings.
target_compile_options(microtex PRIVATE -w)
set_target_properties(microtex PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_CLANG_TIDY "")
