#include "concurrent/Scheduler.hpp"

#include <stdexcept>
#include <utility>

namespace concurrent {

Scheduler::Scheduler(ThreadPool& pool, TimeSource& timeSource)
    : m_pool(pool), m_timeSource(timeSource), m_thread([this](std::stop_token stopToken) { schedulerLoop(stopToken); }) {}

Scheduler::~Scheduler() {
    // Explicit for clarity; the jthread destructor would do the same.
    m_thread.request_stop();
}

TaskId Scheduler::scheduleOnce(const TimeSource::Duration delay, Task task) {
    return add(delay, TimeSource::Duration::zero(), std::move(task));
}

TaskId Scheduler::schedulePeriodic(const TimeSource::Duration interval, Task task) {
    if (interval <= TimeSource::Duration::zero()) {
        throw std::invalid_argument("Scheduler: periodic interval must be positive");
    }
    return add(interval, interval, std::move(task));
}

bool Scheduler::cancel(const TaskId id) {
    std::lock_guard lock(m_mutex);
    const auto it = m_active.find(id);
    if (it == m_active.end()) {
        return false;
    }
    // The heap entry stays and is dropped when it reaches the top.
    it->second->stopSource.request_stop();
    m_active.erase(it);
    return true;
}

TaskId Scheduler::add(const TimeSource::Duration firstDelay, const TimeSource::Duration interval, Task task) {
    if (!task) {
        throw std::invalid_argument("Scheduler: task must not be empty");
    }
    TaskId id = 0;
    {
        std::lock_guard lock(m_mutex);
        id = m_nextId++;
        auto state = std::make_shared<TaskState>(id, std::move(task), interval);
        m_active.emplace(id, state);
        m_heap.push(Entry{m_timeSource.now() + firstDelay, std::move(state)});
    }
    m_cv.notify_one();
    return id;
}

void Scheduler::schedulerLoop(const std::stop_token stopToken) {
    std::vector<std::shared_ptr<TaskState>> due;
    while (!stopToken.stop_requested()) {
        {
            std::unique_lock lock(m_mutex);
            dropCancelledTop();
            if (m_heap.empty()) {
                m_timeSource.wait(m_cv, lock, stopToken, [this] { return !m_heap.empty(); });
                continue;
            }
            const TimeSource::TimePoint deadline = m_heap.top().nextRun;
            const TimeSource::TimePoint now = m_timeSource.now();
            if (deadline > now) {
                // Wakes early only if an entry with an earlier run time was added. Timeout and stop are handled by the loop condition and the clock check above.
                m_timeSource.waitUntil(m_cv, lock, stopToken, deadline, [this, deadline] { return !m_heap.empty() && m_heap.top().nextRun < deadline; });
                continue;
            }
            collectDue(now, due);
        }

        // The mutex is released here, so schedule() and cancel() never wait for the pool queue.
        for (auto& state : due) {
            if (stopToken.stop_requested()) {
                break;
            }
            try {
                (void)m_pool.submit([state = std::move(state)] { state->run(); });
            } catch (...) {
                // An exception must not escape the thread function. The tick is dropped.
            }
        }
        due.clear();
    }
}

void Scheduler::dropCancelledTop() {
    while (!m_heap.empty() && m_heap.top().state->stopSource.stop_requested()) {
        m_heap.pop();
    }
}

void Scheduler::collectDue(const TimeSource::TimePoint now, std::vector<std::shared_ptr<TaskState>>& due) {
    while (!m_heap.empty() && m_heap.top().nextRun <= now) {
        // top() returns a const reference, so the entry cannot be moved out and is copied instead.
        // The copy is cheap because an Entry only holds a time point and a shared_ptr, so just the pointer and its reference count are copied.
        Entry entry = m_heap.top();
        m_heap.pop();
        // dropCancelledTop() only removes cancelled entries at the top, so cancelled entries that lay below a due entry are discarded here.
        if (entry.state->stopSource.stop_requested()) {
            continue;
        }
        if (entry.state->interval > TimeSource::Duration::zero()) {
            // Computed from the planned start, not from now, so that dispatch latency does not accumulate as drift.
            TimeSource::TimePoint next = entry.nextRun + entry.state->interval;
            if (next <= now) {
                // Missed slots are skipped, the next run is the first slot after now.
                next += ((now - next) / entry.state->interval + 1) * entry.state->interval;
            }
            m_heap.push(Entry{next, entry.state});
        } else {
            m_active.erase(entry.state->id);
        }
        due.push_back(std::move(entry.state));
    }
}

void Scheduler::TaskState::run() {
    if (stopSource.stop_requested()) {
        return;
    }
    // Skip the tick if the previous run of this task is still active.
    if (running.exchange(true)) {
        return;
    }
    try {
        task();
    } catch (...) {
        // The scheduler has no one to report to. The task stays scheduled if it is periodic.
        // Catching is also required for correctness: an exception would skip the reset of the running flag below, and a periodic task would then skip all its later ticks.
    }
    running.store(false);
}

} // namespace concurrent
