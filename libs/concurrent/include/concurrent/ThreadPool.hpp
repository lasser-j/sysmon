#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace concurrent {

/// Fixed-size pool of worker threads fed by a single shared FIFO queue.
/// See docs/adr/ADR-0002 and ADR-0003 for the design rationale.
class ThreadPool {
public:
    /// Type-erased unit of work stored in the queue. The result type of the original callable is hidden; results travel through the std::future.
    /// std::function requires its target to be copyable, i.e. all lambda captures must be copyable (see submit()).
    using Task = std::function<void()>;

    /// Starts `threadCount` workers. A count of 0 is raised to 1.
    explicit ThreadPool(std::size_t threadCount = std::thread::hardware_concurrency());

    /// Requests stop and joins all workers. Tasks already queued are still executed.
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /// Enqueues a callable and returns a future for its result.
    /// Exceptions thrown by the callable are rethrown from future::get().
    /// Thread-safe. Must not be called once destruction has begun.
    template <typename F>
    [[nodiscard]] auto submit(F&& callable) -> std::future<std::invoke_result_t<std::decay_t<F>>> {
        using Result = std::invoke_result_t<std::decay_t<F>>;

        // packaged_task links the callable to the future (result or exception).
        // It is not copyable, so a lambda capturing it directly would not be copyable either and std::function would reject it.
        // Capturing a shared_ptr instead keeps the lambda copyable and keeps the packaged_task alive until a worker has run it.
        auto packaged = std::make_shared<std::packaged_task<Result()>>(std::forward<F>(callable));
        auto future = packaged->get_future();  // must happen before the task can run
        enqueue([packaged] { (*packaged)(); });
        return future;
    }

    /// Number of tasks waiting in the queue (excludes running tasks). Approximate.
    [[nodiscard]] std::size_t pendingTasks() const;

    /// Number of worker threads.
    [[nodiscard]] std::size_t threadCount() const noexcept { return m_workers.size(); }

private:
    void enqueue(Task task);
    void workerLoop(std::stop_token stopToken);

    // Declaration order matters: m_workers must be last so that it is destroyed (and joined) before the queue, mutex and condition variable.
    mutable std::mutex m_queueMutex;
    std::condition_variable_any m_queueCv;
    std::queue<Task> m_taskQueue;
    std::vector<std::jthread> m_workers;
};

} // namespace concurrent
