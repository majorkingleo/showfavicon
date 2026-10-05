# The suite grows with the core. Qt6 Test is optional so a machine without it can
# still build the program.
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
