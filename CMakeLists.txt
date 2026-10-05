cmake_minimum_required(VERSION 3.20)

project(ShowFavicon
    VERSION 0.1.0
    DESCRIPTION "Show website favicons in the KDE panel and open the site on click"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Runs moc over the headers and sources with Q_OBJECT, which the QTest classes
# and the QML-facing types need.
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# Lets clangd, cpptools and the editor tooling resolve the exact compile flags.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SHOWFAVICON_WERROR "Treat compiler warnings as errors" OFF)
option(BUILD_TESTING "Build the test suite" ON)

include(GNUInstallDirs)
include(CTest)

# ---------------------------------------------------------------------------
# Qt. Network fetches the page and the icon, Gui decodes and scales them, Svg
# rasterises an SVG favicon.
# ---------------------------------------------------------------------------
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network Gui Svg)

message(STATUS "Qt6        ${Qt6_VERSION}")

add_subdirectory(src)

# The plasmoid is QML and needs no build step. Installing the package tree is
# enough for plasmashell to find it; kpackagetool6 is only for a per user install
# during development.
install(DIRECTORY ${CMAKE_SOURCE_DIR}/plasmoid/com.martin.showfavicon
        DESTINATION ${CMAKE_INSTALL_DATADIR}/plasma/plasmoids)

# The same mark the panel draws, installed under its own name so the icon theme
# can find it.
install(FILES ${CMAKE_SOURCE_DIR}/plasmoid/com.martin.showfavicon/contents/icons/placeholder.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME com.martin.showfavicon.svg)

install(FILES ${CMAKE_SOURCE_DIR}/LICENSE
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/showfavicon
        OPTIONAL)

# The suite grows step by step, so the subdirectory is optional until the first
# C++ test lands.
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
