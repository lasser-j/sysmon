#include "concurrent/ThreadPool.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <future>
#include <latch>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

using concurrent::ThreadPool;
using testing::StrEq;
using testing::ThrowsMessage;

namespace {

// Pool sizes used throughout the tests, named after the role they play.
constexpr std::size_t kSingleWorker = 1;     // default for tests that do not need parallelism, and required for ordering and queue-state tests
constexpr std::size_t kBusyPoolWorkers = 4;  // enough contention for the stress and parallelism tests

// Synchronization uses only latches and futures, never sleeps, so the tests are deterministic and free of data races (TSan-clean).
// A deadlock caused by a pool bug is caught by the ctest timeout.

/// A one-shot gate that blocks any number of waiters until open() is called.
class Gate {
public:
    /// Must be called at most once, a second call would count the underlying latch below zero, which is undefined behavior.
    void open() { m_latch.count_down(); }
    void wait() { m_latch.wait(); }

private:
    std::latch m_latch{1};
};

/// Occupies a single-threaded pool's only worker until release() is called.
/// After construction the worker is guaranteed to be inside the blocking task, so everything submitted afterwards stays queued.
///
/// Two one-shot gates signal in opposite directions:
///   m_started: worker -> test, "the blocking task is running and has left the queue", awaited by the constructor.
///   m_release: test -> worker, "you may finish now", opened by release().
/// The constructor cannot wait on m_release instead of m_started, because release() is only called after the constructor has returned, which would deadlock the test thread.
/// It also cannot rely on m_done, because that future becomes ready only after the task has finished, i.e. after m_release was opened.
class WorkerBlocker {
public:
    explicit WorkerBlocker(ThreadPool& pool) {
        m_done = pool.submit([this] {
            // Must come before the release wait, otherwise the constructor would wait for a signal that is only sent after it returns.
            m_started.open();
            m_release.wait();
        });
        // Without this wait the blocker could still sit in the queue and be counted by pendingTasks().
        // If the worker was faster than this thread, the gate is already open and wait() returns immediately.
        m_started.wait();
    }

    void release() { m_release.open(); }
    void waitUntilDone() { m_done.get(); }

private:
    Gate m_started;
    Gate m_release;
    std::future<void> m_done;
};

} // namespace

TEST(ThreadPoolTest, ReturnsResultThroughFuture) {
    ThreadPool pool(kSingleWorker);

    auto future = pool.submit([] { return 42; });

    EXPECT_EQ(future.get(), 42);
}

TEST(ThreadPoolTest, SupportsVoidTasks) {
    ThreadPool pool(kSingleWorker);
    // Deliberately not atomic: future::get() orders the worker's write before the read below.
    // TSan would flag a pool that failed to provide this ordering.
    bool ran = false;

    auto future = pool.submit([&ran] { ran = true; });
    future.get();

    EXPECT_TRUE(ran);
}

TEST(ThreadPoolTest, AcceptsMoveOnlyCallable) {
    ThreadPool pool(kSingleWorker);

    // The lambda owns a unique_ptr and is therefore move-only, so submit() must move it all the way into the packaged_task.
    auto future = pool.submit([payload = std::make_unique<int>(7)] { return *payload * 6; });

    EXPECT_EQ(future.get(), 42);
}

TEST(ThreadPoolTest, ExceptionIsRethrownFromGet) {
    ThreadPool pool(kSingleWorker);

    auto future = pool.submit([]() -> int { throw std::runtime_error("boom"); });

    // Checking the message as well shows that the original exception reaches the caller and was not replaced by the pool.
    EXPECT_THAT([&future] { (void)future.get(); }, ThrowsMessage<std::runtime_error>(StrEq("boom")));
}

TEST(ThreadPoolTest, WorkerSurvivesThrowingTask) {
    // With a single worker, the follow-up task can only complete if the worker outlived the exception.
    ThreadPool pool(kSingleWorker);

    auto failing = pool.submit([]() -> int { throw std::logic_error("fail"); });
    auto following = pool.submit([] { return 1; });

    EXPECT_THROW((void)failing.get(), std::logic_error);
    EXPECT_EQ(following.get(), 1);
}

TEST(ThreadPoolTest, TasksRunOnWorkerThreads) {
    ThreadPool pool(kSingleWorker);
    const auto callerId = std::this_thread::get_id();

    auto workerId = pool.submit([] { return std::this_thread::get_id(); }).get();

    EXPECT_NE(workerId, callerId);
}

TEST(ThreadPoolTest, ThreadCountReflectsConstructorArgument) {
    // Any count above 1 works; a local constant keeps this test independent of the pool sizes used by the stress tests.
    constexpr std::size_t kRequested = 3;
    ThreadPool pool(kRequested);

    EXPECT_EQ(pool.threadCount(), kRequested);
}

TEST(ThreadPoolTest, ZeroThreadCountIsRaisedToOne) {
    constexpr std::size_t kZero = 0;
    ThreadPool pool(kZero);

    EXPECT_EQ(pool.threadCount(), kSingleWorker);
    EXPECT_EQ(pool.submit([] { return 5; }).get(), 5);
}

TEST(ThreadPoolTest, DefaultConstructedPoolHasAtLeastOneWorker) {
    // The default argument is hardware_concurrency(), which may report 0 on some systems.
    ThreadPool pool;

    EXPECT_GE(pool.threadCount(), 1U);
    EXPECT_EQ(pool.submit([] { return 5; }).get(), 5);
}

TEST(ThreadPoolTest, ExecutesManyTasksExactlyOnce) {
    constexpr std::size_t kTaskCount = 10'000;
    Gate gate;
    ThreadPool pool(kBusyPoolWorkers);
    std::atomic<std::size_t> executions{0};

    // All tasks wait at a gate so the queue is guaranteed to fill up before the workers start draining it.
    std::vector<std::future<std::size_t>> futures;
    futures.reserve(kTaskCount);
    for (std::size_t i = 0; i < kTaskCount; ++i) {
        futures.push_back(pool.submit([i, &gate, &executions] {
            gate.wait();
            ++executions;
            return i;
        }));
    }

    // At most one task per worker can be running, all others must still be queued.
    // EXPECT instead of ASSERT: returning early would leave the workers blocked at the closed gate and hang the pool destructor.
    EXPECT_GE(pool.pendingTasks(), kTaskCount - kBusyPoolWorkers);
    gate.open();

    // Each future must deliver the result of its own task, not of a neighbour.
    for (std::size_t i = 0; i < kTaskCount; ++i) {
        EXPECT_EQ(futures[i].get(), i);
    }
    EXPECT_EQ(executions, kTaskCount);
}

TEST(ThreadPoolTest, WorkersRunConcurrently) {
    // All tasks wait at a latch that opens only when every worker has arrived.
    // Reaching the end therefore proves that the tasks ran in parallel.
    // Without real parallelism the test deadlocks, and the ctest timeout reports that as a failure.
    constexpr std::size_t kThreads = kBusyPoolWorkers;
    std::latch allArrived(static_cast<std::ptrdiff_t>(kThreads));  // declared before the pool so it outlives the workers
    ThreadPool pool(kThreads);

    std::vector<std::future<void>> futures;
    for (std::size_t i = 0; i < kThreads; ++i) {
        futures.push_back(pool.submit([&allArrived] { allArrived.arrive_and_wait(); }));
    }
    for (auto& future : futures) {
        future.get();
    }
}

TEST(ThreadPoolTest, SubmitIsThreadSafe) {
    constexpr std::size_t kProducers = 4;
    constexpr std::size_t kTasksPerProducer = 1'000;
    ThreadPool pool(kBusyPoolWorkers);
    std::atomic<std::size_t> executions{0};

    std::vector<std::vector<std::future<void>>> futuresPerProducer(kProducers);
    {
        std::vector<std::jthread> producers;
        for (std::size_t p = 0; p < kProducers; ++p) {
            producers.emplace_back([&pool, &executions, &futures = futuresPerProducer[p]] {
                for (std::size_t i = 0; i < kTasksPerProducer; ++i) {
                    futures.push_back(pool.submit([&executions] { ++executions; }));
                }
            });
        }
    } // jthread destructors join the producers

    for (auto& futures : futuresPerProducer) {
        for (auto& future : futures) {
            future.get();
        }
    }
    EXPECT_EQ(executions, kProducers * kTasksPerProducer);
}

TEST(ThreadPoolTest, SingleWorkerExecutesInFifoOrder) {
    constexpr int kTaskCount = 100;
    ThreadPool pool(kSingleWorker);
    std::vector<int> executionOrder;  // only touched by the single worker until all futures are ready

    std::vector<std::future<void>> futures;
    for (int i = 0; i < kTaskCount; ++i) {
        futures.push_back(pool.submit([i, &executionOrder] { executionOrder.push_back(i); }));
    }
    for (auto& future : futures) {
        future.get();
    }

    ASSERT_EQ(executionOrder.size(), static_cast<std::size_t>(kTaskCount));
    for (int i = 0; i < kTaskCount; ++i) {
        EXPECT_EQ(executionOrder[static_cast<std::size_t>(i)], i);
    }
}

TEST(ThreadPoolTest, TaskCanSubmitFollowUpTask) {
    // Guards against running tasks while holding the queue mutex, which would deadlock on the nested submit().
    // The inner future is returned instead of awaited inside the task, so this also works on a single worker.
    ThreadPool pool(kSingleWorker);

    auto outer = pool.submit([&pool] { return pool.submit([] { return 99; }); });

    EXPECT_EQ(outer.get().get(), 99);
}

TEST(ThreadPoolTest, PendingTasksCountsQueuedButNotRunningTasks) {
    ThreadPool pool(kSingleWorker);
    EXPECT_EQ(pool.pendingTasks(), 0U);

    WorkerBlocker blocker(pool);
    EXPECT_EQ(pool.pendingTasks(), 0U) << "the running blocker must not count as pending";

    constexpr std::size_t kQueuedTasks = 3;
    std::vector<std::future<void>> futures;
    for (std::size_t i = 0; i < kQueuedTasks; ++i) {
        futures.push_back(pool.submit([] {}));
    }
    EXPECT_EQ(pool.pendingTasks(), kQueuedTasks);

    blocker.release();
    blocker.waitUntilDone();
    for (auto& future : futures) {
        future.get();
    }
    EXPECT_EQ(pool.pendingTasks(), 0U);
}

TEST(ThreadPoolTest, DestructorRunsAllQueuedTasks) {
    constexpr std::size_t kQueuedTasks = 100;
    std::atomic<std::size_t> executions{0};

    {
        auto pool = std::make_unique<ThreadPool>(kSingleWorker);
        WorkerBlocker blocker(*pool);
        for (std::size_t i = 0; i < kQueuedTasks; ++i) {
            (void)pool->submit([&executions] { ++executions; });
        }
        // EXPECT instead of ASSERT: returning early would leave the worker blocked and hang the pool destructor.
        EXPECT_EQ(pool->pendingTasks(), kQueuedTasks);

        // The pool is destroyed on a second thread while its queue is still full.
        // That destructor cannot finish before the blocked worker is released, so the release has to happen inside this scope and right after the thread has been started.
        // Whether the worker starts draining before or after the destructor requests stop, no task may be dropped.
        std::jthread destroyer([&pool] { pool.reset(); });
        blocker.release();
    } // the jthread joins here, after the pool is gone

    // Every task increments the counter exactly once, so reaching kQueuedTasks proves that none was dropped.
    // The discarded futures need no check: a dropped task would only surface in them as std::future_error (broken_promise), which the counter already shows.
    EXPECT_EQ(executions, kQueuedTasks);
}

TEST(ThreadPoolTest, RepeatedCreateAndDestroyLosesNoTasks) {
    // The destructor races with workers that are mid-task or draining the queue, so many short-lived pools cover different interleavings of stop request and task execution.
    // Each task does some real work (atomic increments cannot be optimized away), so the queue is usually still filled and workers are still busy when the destructor starts.
    // The schedule is deliberately not forced: this is a stress test that finds races rather than proving one interleaving, which DestructorRunsAllQueuedTasks covers deterministically.
    constexpr std::size_t kIterations = 100;
    constexpr std::size_t kTasksPerIteration = 50;
    constexpr std::size_t kWorkPerTask = 5'000;
    std::atomic<std::size_t> executions{0};
    std::atomic<std::size_t> workSink{0};

    for (std::size_t i = 0; i < kIterations; ++i) {
        ThreadPool pool(kBusyPoolWorkers);
        for (std::size_t j = 0; j < kTasksPerIteration; ++j) {
            (void)pool.submit([&executions, &workSink] {
                for (std::size_t k = 0; k < kWorkPerTask; ++k) {
                    ++workSink;
                }
                ++executions;
            });
        }
    }

    EXPECT_EQ(executions, kIterations * kTasksPerIteration);
}
