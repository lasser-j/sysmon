# ADR-0003: Worker Thread Management via std::jthread and std::stop_token

**Status:** Accepted
**Date:** 2026-10

## Context

Worker threads in `ThreadPool` and the scheduler thread in `Scheduler` must:

1. Start on construction
2. Shut down cleanly when the owning object is destroyed
3. Receive a cancellation signal without polling a shared flag

C++17 `std::thread` requires manual `join()` and a shared atomic or mutex-protected flag for cooperative shutdown - a common source of bugs.

C++20 introduces `std::jthread`, which joins automatically on destruction and carries a built-in `std::stop_source` / `std::stop_token` mechanism.

## Decision

Use `std::jthread` for all internal threads in `libs/concurrent`. Shutdown is signalled via the implicit `std::stop_source` owned by each `jthread`.

Worker loops use the `std::stop_token` overload of `std::condition_variable_any::wait()`:

```cpp
m_queueCv.wait(lock, stop_token, [this] { return !m_taskQueue.empty(); });
```

This atomically registers a stop callback and releases the lock - the thread wakes on either a new task or a stop request, with no additional primitives required.

## Rationale

- Eliminates `join()`-in-destructor boilerplate that is a common source of subtle lifetime bugs in hand-rolled thread pools.
- The `stop_token` + `condition_variable_any` combination is the idiomatic C++20 cooperative shutdown pattern.
- Demonstrates modern C++ proficiency directly relevant to the portfolio goal.

## Consequences

- Requires C++20. The CMake target sets `target_compile_features(concurrent PUBLIC cxx_std_20)`.
- Destruction order of `ThreadPool` members matters: `m_workers` must be destroyed before `m_queueMutex` and `m_taskQueue`. This is guaranteed by declaring `m_workers` last in the class.
