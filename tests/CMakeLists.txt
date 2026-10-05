# ---------------------------------------------------------------- the QML side
#
# The two things `plasmawindowed` cannot cover: a settings page that fails to load
# takes the whole dialog down without a word, and the drop decisions live in a
# JavaScript module that needs no shell at all. Neither needs Qt6::Test, so this
# section comes before the check that guards the C++ suite.

find_program(QMLSCENE6_EXECUTABLE NAMES qmlscene6)

if(QMLSCENE6_EXECUTABLE)
    # Both harnesses end themselves with Qt.callLater(Qt.quit), because qmlscene6
    # ignores Qt.exit() -- and both report the verdict on stdout, because a QML
    # error does not fail the process on its own.
    add_test(NAME plasmoid-logic
             COMMAND ${QMLSCENE6_EXECUTABLE} -platform offscreen
                     ${CMAKE_CURRENT_SOURCE_DIR}/tst_logic.qml)
    set_tests_properties(plasmoid-logic PROPERTIES
        PASS_REGULAR_EXPRESSION "ALL PASSED"
        FAIL_REGULAR_EXPRESSION "CHECK\\(S\\) FAILED"
        TIMEOUT 120)

    add_test(NAME plasmoid-configpage
             COMMAND ${QMLSCENE6_EXECUTABLE} -platform offscreen
                     ${CMAKE_CURRENT_SOURCE_DIR}/tst_configpage.qml)
    set_tests_properties(plasmoid-configpage PROPERTIES
        PASS_REGULAR_EXPRESSION "ALL PASSED"
        FAIL_REGULAR_EXPRESSION "CHECK\\(S\\) FAILED"
        TIMEOUT 120)
else()
    message(STATUS "qmlscene6 not found, skipping the QML checks")
endif()

# The package's structure, which Plasma never complains about: a config.qml that
# is not a ConfigModel costs the entire settings dialog, and a cfg_ property that
# matches no entry in main.xml is a control that saves nothing.
add_test(NAME plasmoid-structure
         COMMAND ${CMAKE_COMMAND}
                 -DPACKAGE_DIR=${CMAKE_SOURCE_DIR}/plasmoid/com.martin.showfavicon
                 -P ${CMAKE_SOURCE_DIR}/scripts/check-package.cmake)

# -------------------------------------------------------------- the C++ core

# Qt6 Test is optional so a machine without it can still build the program.
find_package(Qt6 6.5 QUIET COMPONENTS Test)

if(NOT Qt6Test_FOUND)
    message(STATUS "Qt6 Test not found, skipping the test suite")
    return()
endif()

# The cache key, the atomic write and the sha1: nothing here needs a network, and
# the widget's grayscale fallback depends on all three.
qt_add_executable(tst_faviconstore tst_faviconstore.cpp)

target_link_libraries(tst_faviconstore
    PRIVATE
        showfavicon-core
        Qt6::Test
)

add_test(NAME faviconstore COMMAND tst_faviconstore)

# Which icon a page advertises. Pure text in, URL out, so the whole ordering can
# be checked without a network.
qt_add_executable(tst_faviconresolver tst_faviconresolver.cpp)

target_link_libraries(tst_faviconresolver
    PRIVATE
        showfavicon-core
        Qt6::Test
)

add_test(NAME faviconresolver COMMAND tst_faviconresolver)

# The HTTP exchange, against a stub server in the test rather than the network:
# redirects, a 404, a refused connection and a hang are all states the widget
# reacts to differently.
qt_add_executable(tst_faviconfetcher tst_faviconfetcher.cpp)

target_link_libraries(tst_faviconfetcher
    PRIVATE
        showfavicon-core
        Qt6::Test
)

add_test(NAME faviconfetcher COMMAND tst_faviconfetcher)

# Decoding, scaling and the grayscale copy. The grayscale one matters most: it is
# what the widget shows while a site is unreachable, and an implementation that
# drops the alpha turns a transparent favicon into a black square.
qt_add_executable(tst_faviconimage tst_faviconimage.cpp)

target_link_libraries(tst_faviconimage
    PRIVATE
        showfavicon-core
        Qt6::Test
)

add_test(NAME faviconimage COMMAND tst_faviconimage)
