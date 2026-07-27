# ADR-0001: Monorepo Structure

**Status:** Accepted
**Date:** 2025-07

## Context

The project consists of two logically separable components:

- `libconcurrent` — a reusable, platform-agnostic concurrency library
- `sysmon` — a Linux system monitoring application that consumes `libconcurrent`

A third cross-cutting concern is the Yocto-based embedded Linux image for deployment on Raspberry Pi.

The question is whether to maintain these as separate repositories or within a single monorepo.

## Decision

**Monorepo.** All components live under a single `sysmon/` root with the following top-level modules:

```
libs/concurrent/        # reusable thread pool and scheduler — no sysmon dependencies
libs/sysmon-types/      # shared data types and interfaces (ICollector, MetricSnapshot)
platform/              # reader interfaces + Linux-specific implementations
transport/             # gRPC service definition, server and client stubs
logging/               # structured logger
apps/sysmon-daemon/    # daemon: MonitorCore + gRPC server, composition root
apps/sysmon-ui/        # Qt/QML and gRPC client
yocto/                 # meta-sysmon Yocto layer for Raspberry Pi image
```

Each module owns its own CMake configuration and test suite.

## Rationale

- A single CMake build graph covers all modules, toolchain files, and CI pipelines without cross-repo coordination.
- Reviewers and potential employers can assess the full project with a single `git clone`.
- The Yocto layer references specific library versions implicitly via the same tree — no submodule pinning required.
- `libconcurrent` remains independently testable and benchmarkable. Its `CMakeLists.txt` exposes a clean `concurrent::concurrent` target without sysmon-specific dependencies.

## Consequences

- All modules share the same CMake minimum version and C++ standard configuration.
- Breaking changes to `libconcurrent`'s public API surface immediately in CI (compilation failure in dependent modules).
- Splitting `libconcurrent` into a standalone repository in the future is straightforward, since the module boundary is already clean.
