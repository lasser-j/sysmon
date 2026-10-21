cmake_minimum_required(VERSION 3.25)

include(GoogleTest)

add_executable(concurrent_tests
    ThreadPoolTest.cpp
)

target_link_libraries(concurrent_tests PRIVATE
    concurrent::concurrent
    GTest::gmock
    GTest::gtest_main
)

target_set_warnings(concurrent_tests PRIVATE)
target_enable_sanitizers(concurrent_tests PRIVATE)

# The prefix makes `ctest -R concurrent` select exactly this module.
# The timeout turns a deadlock (e.g. a broken pool never running a gated task) into a failure instead of a hang.
gtest_discover_tests(concurrent_tests
    TEST_PREFIX "concurrent."
    PROPERTIES TIMEOUT 30
)
