cmake_minimum_required(VERSION 3.25)

project(sysmon
    VERSION 0.1.0
    DESCRIPTION "Linux system monitor with reusable C++20 concurrency library"
    LANGUAGES CXX
)

# Global C++ standard - enforced for all targets in this build
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Shared compiler options (warnings, sanitizers)
include(cmake/CompilerOptions.cmake)

# Build options
option(BUILD_TESTING    "Build test targets"      ON)
option(BUILD_BENCHMARKS "Build benchmark targets" ON)

# Dependencies - fetched only when the corresponding build option is enabled.
# Pinned to specific tags for reproducible builds - update deliberately.
include(FetchContent)

if(BUILD_TESTING)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.17.0
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
endif()

if(BUILD_BENCHMARKS AND NOT CMAKE_CROSSCOMPILING)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.9.5
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googlebenchmark)
endif()

# Modules - uncomment as they are implemented
add_subdirectory(libs/concurrent)
# add_subdirectory(libs/sysmon-types)
# add_subdirectory(platform)
# add_subdirectory(transport)
# add_subdirectory(logging)
# add_subdirectory(apps/sysmon-daemon)
# add_subdirectory(apps/sysmon-ui)
