# Architecture

## Goals

- Demonstrate modern C++ software architecture.
- Provide a reusable concurrency library.
- Separate platform-specific code from domain logic.
- Keep dependencies explicit.
- Maximize testability.
- Avoid unnecessary complexity.

## Design Principles

- Separation of Concerns
- Single Responsibility Principle
- Composition over Inheritance
- Platform Abstraction
- Explicit Dependencies
- Reusable Libraries

## Key Design Decisions

See `docs/adr/` for the full rationale behind each decision.

| Decision | Choice | ADR |
|---|---|---|
| Repository structure | Monorepo | [ADR-0001](adr/ADR-0001-monorepo-structure.md) |

## System Architecture

The project produces two independent binaries with a clear network boundary between them:

- **`sysmon-daemon`** - Headless, no Qt dependency, cross-compiled for aarch64 via Yocto. Runs as a systemd service on the Raspberry Pi.
- **`sysmon-ui`** - Desktop Qt/QML application, connects to the daemon over gRPC. Can run on the Raspberry Pi itself or any machine on the network.

This separation gives gRPC a genuine architectural purpose: It is the network boundary between data collection and visualization, not an internal inter process communication mechanism.

```mermaid
%%{init: {'flowchart': {'curve': 'stepAfter'}}}%%
flowchart LR
  subgraph "sysmon-daemon (Raspberry Pi)"
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

This results in the following modules:

| Module | Responsibility |
|---|---|
| libs/concurrent | Reusable thread pool and scheduler. No sysmon dependencies. |
| libs/sysmon-types | Shared data types and interfaces: ICollector, MetricSnapshot. No behaviour, no platform code. |
| platform/linux | Implements the reader interfaces using /proc and /sys. Depends on libs/sysmon-types for MetricSnapshot. |
| transport | gRPC service definition (proto) and MonitoringServiceImpl. Streams monitoring data to clients. Depends on libs/sysmon-types. |
| logging | Structured logger (spdlog wrapper). No internal dependencies. |
| apps/sysmon-daemon | Composition root: creates MonitorCore (owns Scheduler, readers, and MetricStore) and starts the gRPC server. |
| apps/sysmon-ui | Qt/QML application. Uses gRPC client stubs from transport to receive the metric stream from sysmon-daemon. |

## Dependency Graph

Arrows mean "depends on". Dependency direction is strictly top-down without cycles.

```mermaid
flowchart TD
    APPD["apps/sysmon-daemon"]
    APPU["apps/sysmon-ui"]
    TR["transport"]
    LG["logging"]
    CL["libs/concurrent"]
    PL["platform/linux"]
    TY["libs/sysmon-types"]

    APPD --> CL
    APPD --> PL
    APPD --> TR
    APPD --> LG
    APPU --> TR
    APPU --> LG
    PL   --> TY
    TR   --> TY
```

## Data Flow

How data moves at runtime.

```mermaid
flowchart TD
    PROC["Linux kernel interfaces"]

    subgraph "sysmon-daemon (Raspberry Pi)"
    direction LR
        RD["platform/linux Readers\n(CpuReader · RamReader · ProcessReader)"]
        MC["MonitorCore\n(Scheduler · ThreadPool)"]
        MS["MetricStore\n(Ring buffer)"]
        GS["transport\n(gRPC Server)"]
        RD -->|raw metrics| MC
        MC -->|MetricSnapshot| MS
        MS -->|MetricSnapshot| GS
    end

    subgraph "sysmon-ui (any machine)"
    direction LR
        GC["gRPC Client"]
        UI["Qt/QML UI"]
        GC --> UI
    end

    PROC -->|/proc · /sys| RD
    GS -->|"Metric stream\n(gRPC / HTTP2)"| GC
```

`MonitorCore` writes snapshots to the `MetricStore`, while the `gRPC Server` reads them.
Both components share the `MetricStore` but remain otherwise independent.
