#pragma once

#include "concurrent/TimeSource.hpp"

namespace concurrent {

/// The real time source: std::chrono::steady_clock and the stop_token overloads of condition_variable_any (see ADR-0003).
/// It has no state, so a single instance can be shared by any number of schedulers.
class SteadyTimeSource final : public TimeSource {
public:
    [[nodiscard]] TimePoint now() const override;
    void wait(std::condition_variable_any& cv, std::unique_lock<std::mutex>& lock, std::stop_token stopToken, const std::function<bool()>& predicate) override;
    void waitUntil(std::condition_variable_any& cv, std::unique_lock<std::mutex>& lock, std::stop_token stopToken, TimePoint deadline, const std::function<bool()>& predicate) override;
};

} // namespace concurrent
