#pragma once

#include "concurrent/TimeSource.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <stop_token>

namespace concurrent::test {

/// A TimeSource whose time only moves when the test calls advance().
/// wait() and waitUntil() block the calling thread like the real ones, but they end when the fake time reaches the deadline and not when real time does.
/// awaitIdle() returns once the waiting thread sleeps and nothing would wake it: its predicate is false, the fake time has not reached the deadline and no stop was requested.
/// A Scheduler only sleeps after it has submitted all due tasks, so after advance() and awaitIdle() everything that is due has been dispatched.
///
/// The mutex that is passed to wait() and waitUntil() must outlive every call of advance() and awaitIdle(), because both lock it.
///
/// Two mutexes protect two groups of data:
///   m_waiterMutex points to the mutex of the waiting thread, which protects the sleep state (m_sleeping, m_predicate, m_hasDeadline, m_deadline). The waiting thread holds it when it calls wait() or waitUntil(), advance() locks it briefly before it notifies, and awaitIdle() locks it while it reads the sleep state.
///   m_sleepMutex protects the registration of the waiting thread (m_waiterMutex, m_waiterCv) and the sleep counter (m_sleeps).
/// The lock order is fixed: the waiting thread takes m_sleepMutex while it holds its own mutex. Locking the mutex of the waiting thread while holding m_sleepMutex would reverse this order and could deadlock.
class FakeTimeSource final : public TimeSource {
public:
    explicit FakeTimeSource(const TimePoint start = TimePoint{}) : m_now(start) {}

    [[nodiscard]] TimePoint now() const override { return m_now.load(); }

    /// Moves the time forward and wakes the waiting thread. Must be called by the test thread only.
    void advance(const Duration amount) {
        std::mutex* waiterMutex = nullptr;
        std::condition_variable_any* waiterCv = nullptr;
        {
            std::lock_guard sleepLock(m_sleepMutex);
            m_now.store(m_now.load() + amount);
            waiterMutex = m_waiterMutex;
            waiterCv = m_waiterCv;
        }
        if (waiterMutex == nullptr) {
            // Nobody has waited yet, so a thread that starts waiting later reads the new time itself.
            return;
        }
        // Locking the waiting thread's mutex before notifying closes the lost-wake-up window: that thread holds the mutex from its last check of the time until it sleeps, so the notification cannot fall in between.
        { std::lock_guard waiterLock(*waiterMutex); }
        waiterCv->notify_all();
    }

    /// Blocks until the waiting thread sleeps and nothing would wake it at the current fake time.
    void awaitIdle() {
        while (true) {
            std::uint64_t seenSleeps = 0;
            std::mutex* waiterMutex = nullptr;
            {
                std::lock_guard sleepLock(m_sleepMutex);
                seenSleeps = m_sleeps;
                waiterMutex = m_waiterMutex;
            }
            if (waiterMutex != nullptr) {
                std::lock_guard waiterLock(*waiterMutex);
                if (m_sleeping && !m_predicate() && !(m_hasDeadline && m_now.load() >= m_deadline)) {
                    return;
                }
            }
            // Not idle yet: either the waiting thread is busy or a wake-up is on its way. It goes to sleep again afterwards and counts that, which ends this wait.
            std::unique_lock sleepLock(m_sleepMutex);
            m_sleepCv.wait(sleepLock, [&] { return m_sleeps != seenSleeps; });
        }
    }

    void wait(std::condition_variable_any& waiterCv, std::unique_lock<std::mutex>& waiterLock, const std::stop_token stopToken, const std::function<bool()>& predicate) override {
        sleep(waiterCv, waiterLock, stopToken, false, TimePoint{}, predicate);
    }

    void waitUntil(std::condition_variable_any& waiterCv, std::unique_lock<std::mutex>& waiterLock, const std::stop_token stopToken, const TimePoint deadline, const std::function<bool()>& predicate) override {
        sleep(waiterCv, waiterLock, stopToken, true, deadline, predicate);
    }

private:
    void sleep(std::condition_variable_any& waiterCv, std::unique_lock<std::mutex>& waiterLock, const std::stop_token stopToken, const bool hasDeadline, const TimePoint deadline, const std::function<bool()>& predicate) {
        const auto shouldWake = [&] { return predicate() || (hasDeadline && m_now.load() >= deadline); };
        {
            std::lock_guard sleepLock(m_sleepMutex);
            m_waiterMutex = waiterLock.mutex();
            m_waiterCv = &waiterCv;
        }
        // The caller holds `waiterLock` on entry, as TimeSource::wait() requires, and waiterCv.wait() releases it only when the thread really sleeps.
        // advance() and awaitIdle() lock the same mutex before they notify or look at the sleep state, so they can only see it once the thread waits.
        m_predicate = predicate;
        m_hasDeadline = hasDeadline;
        m_deadline = deadline;
        m_sleeping = true;
        {
            std::lock_guard sleepLock(m_sleepMutex);
            ++m_sleeps;
        }
        m_sleepCv.notify_all();
        waiterCv.wait(waiterLock, stopToken, shouldWake);
        m_sleeping = false;
        m_predicate = nullptr;
    }

    std::atomic<TimePoint> m_now;

    // Counts how often the waiting thread went to sleep. awaitIdle() waits on m_sleepCv until the counter changes.
    std::mutex m_sleepMutex;
    std::condition_variable m_sleepCv;
    std::uint64_t m_sleeps{0};

    // The mutex and condition variable of the waiting thread, registered by sleep() and used by advance() and awaitIdle(). Protected by m_sleepMutex.
    std::mutex* m_waiterMutex{nullptr};
    std::condition_variable_any* m_waiterCv{nullptr};

    // Sleep state of the waiting thread. Protected by the waiting thread's own mutex.
    bool m_sleeping{false};
    bool m_hasDeadline{false};
    TimePoint m_deadline{};
    std::function<bool()> m_predicate;
};

} // namespace concurrent::test
