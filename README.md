# SysMon - A Linux System Monitor

![C++20](https://img.shields.io/badge/C++-20-00599C?&style=flat&logo=cplusplus&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.25+-064F8C?&style=flat&logo=cmake&logoColor=white)
![gRPC](https://img.shields.io/badge/gRPC-Protobuf-4285F4?style=flat&logo=google&logoColor=white)
![Qt](https://img.shields.io/badge/Qt-6-41CD52?style=flat&logo=qt&logoColor=white)

![GoogleTest](https://img.shields.io/badge/GoogleTest-1.17.0-4285F4?style=flat&logo=google&logoColor=white)
![Google Benchmark](https://img.shields.io/badge/Benchmark-1.9.5-4285F4?style=flat&logo=google&logoColor=white)
![spdlog](https://img.shields.io/badge/Logging-spdlog-informational)

![Yocto](https://img.shields.io/badge/Yocto-meta--sysmon-5c9a3e)
![Embedded](https://img.shields.io/badge/Embedded-aarch64-informational)
![QEMU](https://img.shields.io/badge/QEMU-aarch64-FF6600?style=flat&logo=qemu&logoColor=white)

[![CI](https://github.com/lasser-j/sysmon/actions/workflows/ci.yml/badge.svg)](https://github.com/lasser-j/sysmon/actions/workflows/ci.yml)

**SysMon** is a modern C++20 Linux system monitoring application. It demonstrates systems programming by building a reusable C++20 concurrency library, modular software architecture and infrastructure oriented design. The target for this project is aarch64 embedded Linux (as used on devices such as Raspberry Pi), cross-compiled via Yocto and tested in QEMU.

## Features

- **Concurrency Library**
  - Reusable C++20 thread pool and one-shot and periodic task scheduler
  - `std::jthread`-based workers with cooperative shutdown via `std::stop_token`
  - Thread-safe work queue with task submission
  - Google Benchmark suite
- **System Monitoring**
  - CPU, memory and process metrics via `/proc` and `/sys` readers
  - Snapshot based data model
- **Communication**
  - gRPC streaming API with live metric delivery over HTTP/2 + Google Protobuf
  - Independent daemon and UI - The UI can connect locally or remotely
- **User Interface**
  - Qt / QML UI for real-time charts and process table
  - Connects to `sysmon-daemon` via gRPC - runs on any machine on the network
- **Platform**
  - Linux (native and aarch64)
  - Cross-compiled via Yocto
  - Tested in QEMU

## Tech Stack

| Concern | Choice |
|---|---|
| Language | C++20 |
| Build system | CMake 3.25+ |
| Communication | gRPC + Protobuf |
| UI | Qt 6 + QML |
| Testing | GoogleTest |
| Benchmarking | Google Benchmark |
| Logging | spdlog |
| CI | GitHub Actions |
| Embedded | Yocto Project / meta-sysmon |
| Target | aarch64 (QEMU) |

## Architecture

The project builds two independent binaries:

- **`sysmon-daemon`** - Headless monitoring daemon, runs on aarch64 / QEMU, collects system metrics and exposes them via a gRPC server
- **`sysmon-ui`** - Qt/QML client, connects to the daemon via gRPC, runs on any machine

```mermaid
%%{init: {'flowchart': {'curve': 'stepAfter'}}}%%
flowchart LR
  subgraph "sysmon-daemon (aarch64 / QEMU)"
    MC["MonitorCore\nScheduler · ThreadPool"]
    MS["MetricStore"]
    GS["gRPC Server"]
    MC -->|write| MS
    GS -->|read| MS
  end

  subgraph "sysmon-ui (any machine)"
    GC["gRPC Client"]
    UI["Qt/QML UI"]
    UI -->|uses| GC
  end

  GS -->|"gRPC / HTTP2"| GC
```

See [`docs/architecture.md`](docs/architecture.md) for the full module dependency graph and data flow. Design decisions are documented in [`docs/adr/`](docs/adr/).

## Repository Structure

```
sysmon/
├── libs/
│   ├── concurrent/        # ThreadPool + Scheduler (standalone, no sysmon deps)
│   └── sysmon-types/      # Shared domain types and interfaces
├── platform/              # Reader interfaces + Linux-specific /proc implementations
├── transport/             # gRPC service definition, server and client stubs
├── logging/               # Structured logger (leaf module, no internal deps)
├── apps/
│   ├── sysmon-daemon/     # Headless daemon: MonitorCore + gRPC server (aarch64 / QEMU)
│   └── sysmon-ui/         # Qt/QML UI + gRPC client (desktop)
├── yocto/meta-sysmon/     # BitBake recipes for aarch64 embedded Linux image (daemon only) - tested in QEMU
├── cmake/                 # Toolchain files, shared compiler options
└── docs/                  # Architecture docs, ADRs, build guide
```

## Documentation

See the corresponding files inside `docs/`:

| Document | Description |
|---|---|
| [`docs/architecture.md`](docs/architecture.md) | Architecture, module dependency graph and data flow |
| [`docs/concurrent.md`](docs/concurrent.md) | libs/concurrent architecture and internals |
| [`docs/adr/`](docs/adr/) | Architecture Decision Records |
| [`docs/build.md`](docs/build.md) | Build, test and benchmark |

## Status

> **Work in progress.** This section tracks implementation status.

- [x] Repository bootstrap
- [x] `libs/concurrent` - ThreadPool
- [ ] `libs/concurrent` - Scheduler
- [x] CI - GitHub Actions (build, test)
- [ ] `libs/sysmon-types` - MetricSnapshot, interfaces
- [ ] `logging` - structured logger
- [ ] `platform` - CpuReader, RamReader, ProcessReader
- [ ] `transport` - proto definition, gRPC service
- [ ] `apps/sysmon-daemon` - MonitorCore, wiring
- [ ] `apps/sysmon-ui` - Qt/QML client
- [ ] `yocto/meta-sysmon` - aarch64 recipe + QEMU, cross-compile in CI

## AI-Assisted Development

This project is developed with Claude Code as an assistant for implementation, code review and test design. Architecture and design decisions (see [`docs/architecture.md`](docs/architecture.md) and [`docs/adr/`](docs/adr/)) are made by the developer, and every step is reviewed and committed manually. The context given to the agent is in [`CLAUDE.md`](CLAUDE.md).
