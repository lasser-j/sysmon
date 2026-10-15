# Build Guide

## Prerequisites

| Tool | Minimum version |
|---|---|
| CMake | 3.25 |
| GCC | 12 (C++20 required) |
| Clang | 15 (C++20 required) |
| Git | any recent |

### Test and Benchmark Dependencies

GoogleTest and Google Benchmark are fetched automatically via CMake `FetchContent` at configure time. This requires an internet connection on the first build. Subsequent builds use the cached download under `build/_deps/`.

For offline or corporate environments where outbound network access is restricted, point `FetchContent` at local source checkouts:

```bash
cmake -B build \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLEBENCHMARK=/path/to/benchmark
```

## Build Options

| Option | Default | Description |
|---|---|---|
| `CMAKE_BUILD_TYPE` | none | `Debug` or `Release` |
| `BUILD_TESTING` | `ON` | Build test targets (GoogleTest, ASan + UBSan) |
| `BUILD_BENCHMARKS` | `ON` | Build benchmark targets (Google Benchmark, skipped when cross-compiling) |

## Native Build

Development build with tests and benchmarks:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

Production build without tests and benchmarks:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DBUILD_BENCHMARKS=OFF
cmake --build build --parallel
```

Sanitizers are enabled on test targets only.

## Tests

```bash
# All tests
ctest --test-dir build --output-on-failure

# A single module
ctest --test-dir build -R concurrent --output-on-failure
```

## Benchmarks

Benchmarks should be run from a Release build, since Debug timings are not representative:

```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
./build-release/libs/concurrent/benchmarks/concurrent_benchmarks
```
