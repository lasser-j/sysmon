#pragma once

#include "Gate.hpp"

#include "concurrent/ThreadPool.hpp"

#include <stdexcept>

namespace concurrent::test {

/// Occupies a single-threaded pool's only worker until release() is called. The constructor rejects pools with any other worker count.
/// After construction the worker is guaranteed to be inside the blocking task, so everything submitted afterwards stays queued.
///
/// Two one-shot gates signal in opposite directions:
///   m_started: worker -> test, "the blocking task is running and has left the queue", awaited by the constructor.
///   m_release: test -> worker, "you may finish now", opened by release().
/// The constructor cannot wait on m_release instead of m_started, because release() is only called after the constructor has returned, which would deadlock the test thread.
class WorkerBlocker {
public:
    /// Throws std::invalid_argument if the pool does not have exactly one worker, because with more workers the others would keep draining the queue and the guarantee above would not hold.
    explicit WorkerBlocker(ThreadPool& pool) {
        if (pool.threadCount() != 1) {
            throw std::invalid_argument("WorkerBlocker: the pool must have exactly one worker");
        }
        (void)pool.submit([this] {
            // Must come before the release wait, otherwise the constructor would wait for a signal that is only sent after it returns.
            m_started.open();
            m_release.wait();
        });
        // Without this wait the blocker could still sit in the queue and be counted by pendingTasks().
        // If the worker was faster than this thread, the gate is already open and wait() returns immediately.
        m_started.wait();
    }

    void release() { m_release.open(); }

private:
    Gate m_started;
    Gate m_release;
};

} // namespace concurrent::test
