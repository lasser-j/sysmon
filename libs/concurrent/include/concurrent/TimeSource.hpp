#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stop_token>

namespace concurrent {

/// Source of time and of waiting for the Scheduler.
/// The scheduler thread asks it for the current time and lets it do the waiting instead of calling std::chrono and the condition variable itself, so the time can be replaced, for example by a fake that is advanced by hand.
/// The waiting is part of the interface because an implementation that controls the time has to be able to wake the scheduler thread. The scheduler passes its own mutex, condition variable and stop token, so the locking is the same as with the real clock. See docs/adr/ADR-0004.
class TimeSource {
public:
    /// Time point and duration types of the scheduler. They are the types of std::chrono::steady_clock, but a time source does not have to be backed by that clock.
    using TimePoint = std::chrono::steady_clock::time_point;
    using Duration = TimePoint::duration;

    virtual ~TimeSource() = default;

    /// Current time. Called with the scheduler mutex held, so implementations must not call back into the scheduler.
    [[nodiscard]] virtual TimePoint now() const = 0;

    /// Blocks like `cv.wait(lock, stopToken, predicate)`: returns when the predicate is true, when stop is requested, or after a wake-up of `cv` that the implementation chooses to report.
    /// The caller re-checks all conditions after the return and does not rely on a specific reason.
    /// `lock` is locked on entry and on return.
    virtual void wait(std::condition_variable_any& cv, std::unique_lock<std::mutex>& lock, std::stop_token stopToken, const std::function<bool()>& predicate) = 0;

    /// Like wait(), but also returns once `now()` has reached `deadline`.
    virtual void waitUntil(std::condition_variable_any& cv, std::unique_lock<std::mutex>& lock, std::stop_token stopToken, TimePoint deadline, const std::function<bool()>& predicate) = 0;
};

} // namespace concurrent
