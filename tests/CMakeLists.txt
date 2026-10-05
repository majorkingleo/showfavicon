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
