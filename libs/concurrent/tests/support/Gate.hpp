#pragma once

#include <latch>

namespace concurrent::test {

/// A one-shot gate that blocks any number of waiters until open() is called.
class Gate {
public:
    /// Must be called at most once, a second call would count the underlying latch below zero, which is undefined behavior.
    void open() { m_latch.count_down(); }
    void wait() { m_latch.wait(); }

private:
    std::latch m_latch{1};
};

} // namespace concurrent::test
