# libs/concurrent

Reusable C++20 concurrency primitives. No dependencies on sysmon domain types - can be used independently.

## Components

- **`ThreadPool`** - fixed-size pool of worker threads, FIFO task dispatch, returns `std::future<T>` per submitted callable.
- **`Scheduler`** - dispatches one-shot and periodic tasks to an injected `ThreadPool` at configured times.

## Quick Start

```cpp
#include "concurrent/ThreadPool.hpp"
#include "concurrent/Scheduler.hpp"

concurrent::ThreadPool pool{4};

// One-shot submission
auto future = pool.submit([] { return 42; });
int result  = future.get();

// Periodic scheduling
concurrent::Scheduler scheduler{pool};
concurrent::TaskId id = scheduler.schedulePeriodic(
    std::chrono::milliseconds{1000},
    [] { collectMetrics(); }
);
scheduler.cancel(id);
```

## Documentation

- [Architecture and internals](../../docs/concurrent.md)
- [Build, test and benchmark](../../docs/build.md)
- [ADR-0002 - Queue strategy](../../docs/adr/ADR-0002-threadpool-queue-strategy.md)
- [ADR-0003 - Thread management](../../docs/adr/ADR-0003-jthread-stop-token.md)
- [ADR-0004 - Scheduler design](../../docs/adr/ADR-0004-scheduler-design.md)
