#include "concurrent/Scheduler.hpp"

#include "support/FakeTimeSource.hpp"
#include "support/WorkerBlocker.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <future>

using concurrent::Scheduler;
using concurrent::ThreadPool;
using concurrent::test::FakeTimeSource;
using concurrent::test::WorkerBlocker;
using namespace std::chrono_literals;

namespace {

constexpr std::size_t kSingleWorker = 1;  // WorkerBlocker requires a pool with exactly one worker

} // namespace

// These tests check that the scheduler takes its time and its waiting from the injected time source.
// The behaviour of the scheduler itself is tested in SchedulerTest.cpp.
//
// The single worker of the pool is blocked, so every task the scheduler dispatches stays in the pool queue and pendingTasks() counts the dispatches exactly.
//
// Lifetime: the fake and the pool are declared before the scheduler, so the scheduler is destroyed first. The blocker is released before the end of each test, because the pool runs everything that is queued when it is destroyed.

TEST(SchedulerInjectionTest, OneShotIsDispatchedOnlyWhenFakeTimeReachesTheDelay) {
    FakeTimeSource time;
    ThreadPool pool(kSingleWorker);
    WorkerBlocker blocker(pool);
    Scheduler scheduler(pool, time);

    scheduler.scheduleOnce(10ms, [] {});

    time.advance(9ms);
    time.awaitIdle();
    EXPECT_EQ(pool.pendingTasks(), 0u);
    time.advance(1ms);
    time.awaitIdle();
    EXPECT_EQ(pool.pendingTasks(), 1u);

    blocker.release();
}

TEST(SchedulerInjectionTest, TaskThatIsDueNowIsDispatchedWithoutAdvancingTime) {
    FakeTimeSource time;
    ThreadPool pool(kSingleWorker);
    WorkerBlocker blocker(pool);
    Scheduler scheduler(pool, time);

    scheduler.scheduleOnce(0ms, [] {});
    time.awaitIdle();

    EXPECT_EQ(pool.pendingTasks(), 1u);

    blocker.release();
}

TEST(SchedulerInjectionTest, PeriodicTaskIsDispatchedOncePerInterval) {
    FakeTimeSource time;
    ThreadPool pool(kSingleWorker);
    WorkerBlocker blocker(pool);
    Scheduler scheduler(pool, time);

    scheduler.schedulePeriodic(10ms, [] {});

    time.advance(10ms);
    time.awaitIdle();
    EXPECT_EQ(pool.pendingTasks(), 1u);
    time.advance(5ms);
    time.awaitIdle();
    EXPECT_EQ(pool.pendingTasks(), 1u);
    time.advance(5ms);
    time.awaitIdle();
    EXPECT_EQ(pool.pendingTasks(), 2u);

    blocker.release();
}

TEST(SchedulerInjectionTest, TaskRunsOnThePoolWhenFakeTimeReachesTheDelay) {
    std::promise<void> ran;
    FakeTimeSource time;
    ThreadPool pool(kSingleWorker);
    Scheduler scheduler(pool, time);

    scheduler.scheduleOnce(1h, [&] { ran.set_value(); });
    time.advance(1h);

    // Without the injected time source this would take an hour, a hang is caught by the ctest timeout.
    ran.get_future().get();
}
