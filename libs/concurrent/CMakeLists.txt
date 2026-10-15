cmake_minimum_required(VERSION 3.25)

# Library target
# add_library produces a linkable library, not a standalone binary.
# sysmon-daemon links against concurrent::concurrent as a dependency.
add_library(concurrent STATIC
    #src/ThreadPool.cpp
    #src/Scheduler.cpp
)

add_library(concurrent::concurrent ALIAS concurrent)

target_include_directories(concurrent PUBLIC include)

target_compile_features(concurrent PUBLIC cxx_std_20)
target_set_warnings(concurrent PRIVATE)

# Tests - excluded from production and cross-compiled builds
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# Benchmarks - excluded from production and cross-compiled builds
if(BUILD_BENCHMARKS AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(benchmarks)
endif()