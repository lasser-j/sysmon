#pragma once

#include "concurrent/ThreadPool.hpp"

#include <atomic>
#include <chrono>
#include <compare>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <vector>

namespace concurrent {

/// Identifies a scheduled task. Ids are assigned in increasing order and never reused.
using TaskId = std::uint64_t;

/// Dispatches one-shot and periodic tasks to an injected ThreadPool at their due time.
/// A single scheduler thread waits on a min-heap ordered by the next run time and submits due tasks to the pool.
/// See docs/adr/ADR-0004 for the design rationale.
class Scheduler {
public:
    using Clock = std::chrono::steady_clock;
    using Task = ThreadPool::Task;

    /// The pool is not owned and must outlive the scheduler.
    explicit Scheduler(ThreadPool& pool);

    /// Requests stop and joins the scheduler thread. Tasks still waiting in the heap are discarded, tasks already submitted to the pool are not affected.
    ~Scheduler();

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    Scheduler(Scheduler&&) = delete;
    Scheduler& operator=(Scheduler&&) = delete;

    /// Runs `task` once on the pool after `delay`. A zero or negative delay means due immediately.
    /// Throws std::invalid_argument if `task` is empty.
    /// Exceptions thrown by the task are discarded.
    /// Thread-safe. Must not be called once destruction has begun.
    TaskId scheduleOnce(Clock::duration delay, Task task);

    /// Runs `task` on the pool every `interval`, the first run is one interval after the call.
    /// The next run is computed from the planned start, so dispatch latency does not accumulate as drift. Slots that were missed, e.g. after a suspend, are skipped.
    /// If the previous run is still active when a tick is due, the tick is skipped, so a task never overlaps with itself.
    /// Throws std::invalid_argument if `interval` is not positive or `task` is empty.
    /// Exceptions thrown by the task are discarded and do not stop the periodic task.
    /// Thread-safe. Must not be called once destruction has begun.
    TaskId schedulePeriodic(Clock::duration interval, Task task);

    /// Prevents further runs of the task and returns true if it was still active.
    /// A task is active if it is a one-shot task that was not dispatched yet or a periodic task that was not cancelled yet. A periodic task stays active after each dispatch.
    /// Returns false for unknown ids, for tasks that were already cancelled and for one-shot tasks that were already dispatched.
    /// A run that has already started is not interrupted. A run of a periodic task that was already submitted to the pool but has not started yet is skipped, whereas a dispatched one-shot task can no longer be cancelled.
    /// Never waits for a running task, so it may be called from inside a task callback.
    /// Thread-safe.
    bool cancel(TaskId id);

private:
    /// State shared between the heap, the active map and the wrappers running on the pool (the callables the scheduler submits to the pool, see ADR-0004).
    /// The wrappers hold only this state and never the scheduler, so they stay valid after the scheduler is destroyed.
    struct TaskState {
        TaskState(const TaskId taskId, Task callback, const Clock::duration repeatInterval)
            : id(taskId), task(std::move(callback)), interval(repeatInterval) {}

        const TaskId id;
        const Task task;
        /// Zero for one-shot tasks.
        const Clock::duration interval;
        /// Per-task cancellation, separate from the stop token of the scheduler thread. cancel() requests stop, the scheduler and the pool wrapper check it before they act on the task.
        std::stop_source stopSource;
        std::atomic<bool> running{false};

        /// Body of the wrapper that runs on the pool: skips the run if the task was cancelled or the previous run is still active, otherwise calls the task and discards its exceptions.
        /// It is a member of the state and not of the scheduler, so it cannot touch the scheduler, which may already be destroyed when the wrapper runs.
        void run();
    };

    struct Entry {
        Clock::time_point nextRun;
        std::shared_ptr<TaskState> state;

        /// Orders entries by run time. Entries with the same time are ordered by id, i.e. in the order they were scheduled, so the dispatch order is deterministic.
        /// The id is compared explicitly because comparing the shared_ptr would compare addresses.
        auto operator<=>(const Entry& other) const {
            if (const auto byTime = nextRun <=> other.nextRun; byTime != 0) {
                return byTime;
            }
            return state->id <=> other.state->id;
        }
    };

    TaskId add(Clock::duration firstDelay, Clock::duration interval, Task task);
    void schedulerLoop(std::stop_token stopToken);
    void dropCancelledTop();
    void collectDue(Clock::time_point now, std::vector<std::shared_ptr<TaskState>>& due);

    ThreadPool& m_pool;

    // Declaration order matters: m_thread must be last so that it is destroyed (and joined) before the mutex, condition variable, heap and map.
    std::mutex m_mutex;
    std::condition_variable_any m_cv;
    // std::greater turns the max-heap of the priority_queue into a min-heap, so the earliest entry is on top.
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> m_heap;
    std::unordered_map<TaskId, std::shared_ptr<TaskState>> m_active;
    TaskId m_nextId{1};
    std::jthread m_thread;
};

} // namespace concurrent
