# The commands are a library of their own so tests can run them.
add_library(jotcli STATIC
    Completion.h
    Completion.cpp
    VaultCommands.h
    VaultCommands.cpp
)
target_include_directories(jotcli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(jotcli PUBLIC Qt6::Core Qt6::Network jotcore)

qt_add_executable(jot
    main.cpp
)

# Printed by jot completions, and installed where each shell looks.
qt_add_resources(jot completions
    PREFIX /completions
    BASE ${PROJECT_SOURCE_DIR}/data/completions
    FILES
        ${PROJECT_SOURCE_DIR}/data/completions/jot.bash
        ${PROJECT_SOURCE_DIR}/data/completions/_jot
        ${PROJECT_SOURCE_DIR}/data/completions/jot.fish
)
install(FILES ${PROJECT_SOURCE_DIR}/data/completions/jot.bash
    DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions RENAME jot)
install(FILES ${PROJECT_SOURCE_DIR}/data/completions/_jot
    DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions)
install(FILES ${PROJECT_SOURCE_DIR}/data/completions/jot.fish
    DESTINATION ${CMAKE_INSTALL_DATADIR}/fish/vendor_completions.d)

target_compile_definitions(jot PRIVATE JOT_VERSION="${PROJECT_VERSION}")
target_link_libraries(jot PRIVATE Qt6::Core jotcore jotcli)

install(TARGETS jot RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
