#pragma once

#include "Gate.hpp"

#include <atomic>
#include <cstddef>

namespace concurrent::test {

/// Counts hits and opens a gate when the target is reached.
/// More hits than the target are fine, the gate opens exactly once.
class RunCounter {
public:
    explicit RunCounter(const std::size_t target) : m_target(target) {}

    void hit() {
        // The atomic increment returns the new value, so every hit sees its own number and exactly one hit sees the target.
        if (++m_count == m_target) {
            m_reached.open();
        }
    }
    void waitUntilReached() { m_reached.wait(); }
    [[nodiscard]] std::size_t count() const { return m_count.load(); }

private:
    const std::size_t m_target;
    std::atomic<std::size_t> m_count{0};
    Gate m_reached;
};

} // namespace concurrent::test
