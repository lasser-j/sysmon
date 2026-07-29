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

For offline or corporate environments where outbound network access is restricted, the dependencies can be provided via a vcpkg / Conan package manager instead:

```bash
# Example: provide via vcpkg
cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

## Native Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

For a debug build with sanitizers enabled on test targets:

```bash
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --parallel
```
