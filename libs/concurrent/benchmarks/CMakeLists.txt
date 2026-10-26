cmake_minimum_required(VERSION 3.25)

# Benchmarks link the regular library, never concurrent_sanitized, because sanitizer overhead would distort the timings.
add_executable(concurrent_benchmarks
    ThreadPoolBenchmark.cpp
)

target_link_libraries(concurrent_benchmarks PRIVATE
    concurrent::concurrent
    benchmark::benchmark_main
)

target_set_warnings(concurrent_benchmarks PRIVATE)
