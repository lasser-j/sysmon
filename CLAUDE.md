# SysMon - Claude Code Project Context

## Project Overview

Modern C++20 Linux system monitoring application and reusable concurrency library.
Target: aarch64 embedded Linux, cross-compiled via Yocto, tested in QEMU.

Two deployable binaries:
- `sysmon-daemon` - headless, cross-compiled for aarch64, exposes gRPC server
- `sysmon-ui` - Qt/QML desktop client, connects to daemon via gRPC

## Repository Structure

```
sysmon/
├── libs/
│   ├── concurrent/        # ThreadPool + Scheduler (standalone, no sysmon deps)
│   └── sysmon-types/      # Shared domain types and interfaces (no behaviour)
├── platform/              # Reader interfaces + Linux-specific /proc implementations
├── transport/             # gRPC service definition, server and client stubs
├── logging/               # Structured logger (leaf module, no internal deps)
├── apps/
│   ├── sysmon-daemon/     # Headless daemon: MonitorCore + gRPC server (aarch64)
│   └── sysmon-ui/         # Qt/QML gRPC client (desktop)
├── yocto/meta-sysmon/     # BitBake recipes for aarch64 embedded Linux image
├── cmake/                 # Toolchain files, shared compiler options
└── docs/                  # Architecture docs, ADRs, build guide
```

## Dependency Direction

Strictly top-down, no cycles:

```
apps/sysmon-daemon --> transport --> libs/sysmon-types
apps/sysmon-daemon --> platform --> libs/sysmon-types
apps/sysmon-daemon --> libs/concurrent
apps/sysmon-daemon --> logging
apps/sysmon-ui     --> transport
apps/sysmon-ui     --> logging
```

## Coding Conventions

- **Language:** C++20, all code and comments in English
- **Member variables:** `m_` prefix (e.g. `m_taskQueue`, `m_workers`)
- **Methods:** camelCase (e.g. `submit()`, `scheduleOnce()`, `pendingTasks()`)
- **Classes:** PascalCase (e.g. `ThreadPool`, `Scheduler`)
- **Namespaces:** lowercase (e.g. `concurrent`, `sysmon`)
- **Files:** PascalCase, named after the primary class they contain (e.g. `ThreadPool.hpp`, `ThreadPool.cpp`, `CpuReaderTest.cpp`). Exception: `main.cpp`
- **CMake functions:** snake_case (consistent with CMake stdlib)
- **No trailing underscores** on member variables
- **Code comments:** No artificial line breaks mid-sentence; split into two sentences instead, but do not shorten sentences artificially either. Long lines are fine

## Markdown Conventions

- No horizontal rules (`---`)
- No em-dashes (`—`), use regular hyphens (`-`)
- No artificial line breaks: a paragraph is one single line, also between sentences
- Mermaid for all diagrams

## Testing

- Framework: GoogleTest
- Test targets are built with ASan + UBSan by default, or with TSan via `-DSANITIZER=thread` in a separate build directory (via `target_enable_sanitizers()` in cmake/CompilerOptions.cmake)
- Sanitizer flags only affect the files a target compiles itself, so each module's tests link against a second build of the library sources with the flags (e.g. `concurrent_sanitized`) instead of the regular library target
- Tests do not use sleeps for synchronization, they use latches and futures
- Each module has its own tests/ subdirectory with its own CMakeLists.txt
- Benchmarks use Google Benchmark

## Workflow

Implement one step at a time. Each step is reviewed and committed before the next begins. Do not implement multiple modules or files beyond what is explicitly requested in the current step.

Planned module order:
1. libs/concurrent (interfaces, implementation, tests, benchmarks)
2. CI - GitHub Actions (build + test for libs/concurrent)
3. libs/sysmon-types
4. logging
5. platform
6. transport
7. apps/sysmon-daemon
8. apps/sysmon-ui
9. yocto/meta-sysmon + QEMU

## Build and Test Commands

CMakePresets.json defines the `dev` and `tsan` presets with the Ninja generator. They were added for VS Code (CMake Tools), where they appear as selectable configurations, and they work the same in the terminal.

### Development build (ASan + UBSan)

```bash
cmake --preset dev
cmake --build --preset dev --parallel

# All tests
ctest --preset dev

# Single module
ctest --preset dev -R concurrent
```

### ThreadSanitizer build

Separate directory, because ASan and TSan cannot be combined. On WSL2, prefix the build and the ctest call with `setarch -R`.

```bash
cmake --preset tsan
cmake --build --preset tsan --parallel
ctest --preset tsan
```

### Production build (no tests, no benchmarks)

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DBUILD_BENCHMARKS=OFF
cmake --build build --parallel
```

### Benchmarks

The `release` preset builds only the benchmarks (Release, no tests) into `build-release/`. The `dev` and `tsan` presets set `BUILD_BENCHMARKS=OFF`, because timings from sanitized Debug builds are not representative.

```bash
cmake --preset release
cmake --build --preset release --parallel
./build-release/libs/concurrent/benchmarks/concurrent_benchmarks
```
