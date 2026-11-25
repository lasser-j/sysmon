#include "concurrent/SteadyTimeSource.hpp"

namespace concurrent {

TimeSource::TimePoint SteadyTimeSource::now() const {
    return std::chrono::steady_clock::now();
}

void SteadyTimeSource::wait(std::condition_variable_any& cv, std::unique_lock<std::mutex>& lock, const std::stop_token stopToken, const std::function<bool()>& predicate) {
    cv.wait(lock, stopToken, predicate);
}

void SteadyTimeSource::waitUntil(std::condition_variable_any& cv, std::unique_lock<std::mutex>& lock, const std::stop_token stopToken, const TimePoint deadline, const std::function<bool()>& predicate) {
    cv.wait_until(lock, stopToken, deadline, predicate);
}

} // namespace concurrent
