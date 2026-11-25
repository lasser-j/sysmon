# libs/concurrent

Reusable C++20 concurrency primitives. No dependencies on sysmon domain types - can be used independently.

## Components

- **`ThreadPool`** - fixed-size pool of worker threads, FIFO task dispatch, returns `std::future<T>` per submitted callable.
- **`Scheduler`** - dispatches one-shot and periodic tasks to an injected `ThreadPool` at configured times. The time source is injected as well, `SteadyTimeSource` is the implementation for real time and tests can pass a fake.

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

## Tests

The tests of both components run against real threads. The `Scheduler` tests in `SchedulerTest.cpp` are integration tests: they use a real `ThreadPool` and the real `steady_clock` with short intervals. The time is injectable now (`TimeSource`), with a fake time source in `tests/support/`. Moving the tests to it is planned, `SchedulerInjectionTest.cpp` only checks the injection itself.

Tests never synchronize with sleeps, they wait on latches, futures and gates. Times are only checked against lower bounds, so a slow CI machine can delay a test but not make it fail. A hang caused by a bug is turned into a failure by the ctest timeout. The test targets are built with ASan and UBSan by default, and with TSan in the `tsan` preset, see [docs/build.md](../../docs/build.md) for the commands and the `setarch -R` note for WSL2. In the single-worker tests the variables shared with callbacks are deliberately plain and not atomic, so that TSan checks the ordering that the futures and gates provide. The conventions are written down at the top of `SchedulerTest.cpp`.

What the tests cover, by topic:

| Component | Topic | What is checked |
|---|---|---|
| `ThreadPool` | Results | Values and void tasks through the future, move-only callables, exceptions rethrown from `get()`, the worker survives a throwing task |
| `ThreadPool` | Sizing | Thread count, a count of 0 is raised to 1, the default has at least one worker |
| `ThreadPool` | Execution | Every task runs exactly once, tasks run in parallel, a single worker runs in FIFO order, a task may submit a follow-up task, `pendingTasks()` counts queued but not running tasks |
| `ThreadPool` | Concurrency and shutdown | Concurrent `submit()`, the destructor runs all queued tasks, repeated create and destroy loses no tasks |
| `Scheduler` | One-shot and periodic | Runs exactly once and not before the delay, zero and negative delays are due at once, the first periodic run is one interval after the call, ids are unique and increasing |
| `Scheduler` | `cancel()` | Return value for active, unknown, already cancelled and dispatched tasks, a run waiting in the pool queue is skipped for periodic and still happens for one-shot tasks, a running callback is neither waited for nor interrupted, self-cancel from the callback |
| `Scheduler` | Overlap and ordering | A slow periodic task never runs concurrently with itself, tasks run in due-time order, a later inserted earlier task wakes the scheduler thread, equal delays are dispatched in scheduling order |
| `Scheduler` | Exceptions and arguments | A periodic task keeps running after an exception, a one-shot task is done and the scheduler keeps working, invalid intervals and empty tasks throw `std::invalid_argument` and schedule nothing |
| `Scheduler` | Destruction | The destructor does not wait for tasks in the heap and they never run, tasks already submitted to the pool finish after the scheduler is gone, a callback may schedule and cancel other tasks |
| `Scheduler` | Concurrency | Many threads schedule and cancel at the same time, every task that was not cancelled runs once, ids stay unique |

### Known gaps

- `DISABLED_PeriodicTaskDoesNotBurstAfterThePoolWasBlocked` documents a deviation from the described catch-up behaviour. While the pool is blocked, the scheduler thread keeps dispatching, so every missed tick ends up as its own queued run and all of them execute back to back once the pool is released. The test is disabled until the behaviour is decided.
- The arithmetic that skips missed slots after a delayed scheduler thread is not tested yet. It can be tested deterministically with the injectable clock, which is planned.
- The `catch` around `pool.submit()` in the scheduler thread, which drops a tick if `submit()` throws, is not tested yet, because the real `ThreadPool` cannot be made to throw. It needs an injectable executor.
- That `cancel()` prevents the run of a one-shot task that is not yet due is not tested yet, because a short delay would make the test depend on how fast the test thread reaches `cancel()`. With the fake clock it can be tested deterministically, which is planned.

### Test helpers

The helpers in `tests/support/` are shared by the test files. `Gate` blocks callbacks until it is opened, `RunCounter` waits until a task has run a given number of times, and `WorkerBlocker` occupies the only worker of a single-worker pool so that everything submitted afterwards stays queued.

## Benchmarks

The `ThreadPool` benchmarks live in `benchmarks/` and are built by the `release` preset, see [docs/build.md](../../docs/build.md). Each one answers a specific question:

| Benchmark | Question it answers |
|---|---|
| `BM_SubmitThroughput` | How many empty tasks per second can the pool handle, depending on the worker count? This is the pure pool overhead. |
| `BM_ThreadPerTask` | How fast is the naive alternative of starting a new thread per task via `std::async`? It is the reference for `BM_SubmitThroughput`, but not at equal parallelism, since every task of a batch gets its own thread. |
| `BM_RoundTripLatency` | How long does a single task take from `submit()` to `get()` on an otherwise idle pool? |
| `BM_Scaling` | How close does the pool get to the ideal speedup when the worker count grows, for fine-grained and coarse-grained tasks? The two task durations are the constants `kFineTaskDuration` and `kCoarseTaskDuration` in the benchmark. |
| `BM_MultiProducer` | What happens when several threads submit into the same pool at once? |

### Reading the output

Only the `Time` column (wall-clock time per iteration) and `items_per_second` are meaningful, because the benchmarks use real time. The `CPU` column counts only the calling thread and not the workers, and `Iterations` is chosen by Google Benchmark. The numbers after the benchmark name are its arguments, for example `BM_Scaling/4/100000` is 4 workers and a task duration of 100000 ns.

One iteration is a batch of `kBatchSize` tasks that are submitted and then awaited, so the time shown is per batch and not per task. `items_per_second` is derived from that batch size. All batch benchmarks are implemented and were run for this one fixed batch size, so results for much smaller or larger batches, for example with a deeper queue, can differ. The only exception is `BM_RoundTripLatency`, where one iteration is a single task. For `BM_Scaling` the ideal batch time is `kBatchSize` times the task duration divided by the worker count, and the ratio of ideal to measured time is the efficiency of the pool.

### Interpreting the results

The throughput with empty tasks (`items_per_second`) is an upper bound that consists of pool overhead only. Its reciprocal is the overhead per task, for example 1M tasks/s means roughly 1 us per task. The overhead depends on the worker count, because producers and workers compete for the shared queue and sleeping workers have to be woken up, so the value for the worker count in use is the relevant one.

As a rule of thumb, a task should run 10 to 100 times longer than the overhead per task, otherwise the pool spends more time on bookkeeping than on the work itself. Shorter units of work are better batched into fewer, larger tasks.

What to look at in each benchmark, and which decision it informs:

- **Efficiency in `BM_Scaling` with coarse tasks:** It should stay close to constant as workers are added. A drop shows the worker count beyond which more threads no longer help, which is the upper limit for a sensible pool size on that hardware.
- **Behaviour in `BM_Scaling` with fine tasks:** If more workers make the batch slower, the shared queue is the bottleneck for tasks of that size. The consequence is to batch such work into larger tasks, or to consider a different queue design such as one queue per worker with work stealing.
- **Gap between `BM_SubmitThroughput` and `BM_ThreadPerTask`:** It shows how much the pool is worth when the work per task is negligible and the per-task overhead dominates. If the gap is small even then, a pool is not justified for work like this.
- **`BM_RoundTripLatency`:** It is the minimum cost of waiting for a single result and is dominated by thread wakeups of the kernel. It matters wherever a caller blocks on a result, and should not depend on the pool size.
- **`BM_MultiProducer`:** If the throughput falls as producers are added, the lock on the queue is the limit. If it rises, as long as the queue stays filled and workers do not go to sleep, wakeups are the dominant cost and not the lock.

The benchmarks also serve as a regression check: after a change to the queue, the task type or the synchronization, run the same benchmarks again and compare the shape of the curves and the ratios, not single values. Use `--benchmark_repetitions` and compare the result only if the difference is larger than the standard deviation.

Absolute numbers are only meaningful on the target hardware. QEMU emulation distorts timings, atomics and thread scheduling unevenly, so results from it are not comparable. Numbers from a development machine are suitable for spotting regressions after a change, not for sizing tasks on the target.

## Documentation

- [Architecture and internals](../../docs/concurrent.md)
- [Build, test and benchmark](../../docs/build.md)
- [ADR-0002 - Queue strategy](../../docs/adr/ADR-0002-threadpool-queue-strategy.md)
- [ADR-0003 - Thread management](../../docs/adr/ADR-0003-jthread-stop-token.md)
- [ADR-0004 - Scheduler design](../../docs/adr/ADR-0004-scheduler-design.md)
