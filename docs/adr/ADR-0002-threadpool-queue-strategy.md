# ADR-0002: ThreadPool Queue Strategy

**Status:** Accepted
**Date:** 2025-07

## Context

The `ThreadPool` requires a mechanism to distribute tasks across worker threads. Two primary strategies exist:

**Option A - Single shared MPMC queue**
All workers share one queue protected by a mutex and a condition variable.

**Option B - Per-thread deque with work-stealing**
Each worker has a local deque. Idle workers steal tasks from the back of busy workers' deques (Chase-Lev algorithm). Used by Intel TBB and the Go runtime scheduler.

## Decision

**Option A - Single shared MPMC queue** using `std::deque<Task>` + `std::mutex` + `std::condition_variable_any`.

## Rationale

Tasks submitted to the `ThreadPool` in this project are collector callbacks: short-lived, I/O-bound reads from `/proc` and `/sys`. They are:

- Uniform in duration (microseconds to low milliseconds)
- Submitted at a low, predictable rate (every 500 ms–2 s per collector)
- Not CPU-bound - no benefit from cache-local execution on a specific core

Under these conditions, contention on the shared queue is negligible. Work-stealing provides meaningful benefit only for CPU-bound workloads with heterogeneous task durations. Introducing it here would add significant implementation complexity (atomic deque operations, ABA problem mitigations) with no measurable runtime benefit.

`std::condition_variable_any` (over `std::condition_variable`) is used to integrate cleanly with `std::stop_token` - workers wake on either a new task or a stop request without an additional synchronisation primitive.

## Consequences

- Implementation is straightforward and auditable.
- The queue strategy can be replaced behind the existing `ThreadPool` public   interface without changes to callers if requirements change.
- Work-stealing via per-thread lock-free deques is a documented future   evolution path if CPU-bound workloads are introduced.
