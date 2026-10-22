cmake_minimum_required(VERSION 3.25)

include(GoogleTest)

# Sanitizer flags only affect the files compiled by the target that carries them.
# The tests therefore link against a second build of the library sources with sanitizer flags, so that the code under test is checked as well and not just the test code.
# The regular concurrent target stays untouched for the daemon and the benchmarks.
add_library(concurrent_sanitized STATIC ${CONCURRENT_SOURCES})
target_include_directories(concurrent_sanitized PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_compile_features(concurrent_sanitized PUBLIC cxx_std_20)
target_set_warnings(concurrent_sanitized PRIVATE)
target_enable_sanitizers(concurrent_sanitized PUBLIC)

add_executable(concurrent_tests
    ThreadPoolTest.cpp
)

target_link_libraries(concurrent_tests PRIVATE
    concurrent_sanitized
    GTest::gmock
    GTest::gtest_main
)

# The sanitizer flags reach this target through the PUBLIC flags of concurrent_sanitized.
target_set_warnings(concurrent_tests PRIVATE)

# The prefix makes `ctest -R concurrent` select exactly this module.
# The timeout turns a deadlock (e.g. a broken pool never running a gated task) into a failure instead of a hang.
gtest_discover_tests(concurrent_tests
    TEST_PREFIX "concurrent."
    PROPERTIES TIMEOUT 30
)
