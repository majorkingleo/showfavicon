# ---------------------------------------------------------------------------
# The favicon core: no widgets, no Plasma. Qt Core for the types, Network for the
# fetches, Gui for decoding and scaling, Svg for a favicon that is an SVG.
# ---------------------------------------------------------------------------

add_library(showfavicon-core STATIC
    log.cpp
    paths.cpp
)

target_include_directories(showfavicon-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(showfavicon-core
    PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::Gui
        Qt6::Svg
)

if(SHOWFAVICON_WERROR)
    target_compile_options(showfavicon-core PRIVATE -Wall -Wextra -Werror)
else()
    target_compile_options(showfavicon-core PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------

configure_file(version.h.in version.h @ONLY)

add_executable(showfavicon
    main.cpp
    cli.cpp
)

target_link_libraries(showfavicon PRIVATE showfavicon-core)

# version.h is generated into the build directory, not the source tree.
target_include_directories(showfavicon PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(SHOWFAVICON_WERROR)
    target_compile_options(showfavicon PRIVATE -Wall -Wextra -Werror)
else()
    target_compile_options(showfavicon PRIVATE -Wall -Wextra)
endif()

install(TARGETS showfavicon
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
