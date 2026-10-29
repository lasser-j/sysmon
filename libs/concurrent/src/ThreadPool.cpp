#include "concurrent/ThreadPool.hpp"

#include <algorithm>

namespace concurrent {

ThreadPool::ThreadPool(const std::size_t threadCount) {
    const std::size_t count = std::max<std::size_t>(threadCount, 1);
    m_workers.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        m_workers.emplace_back([this](std::stop_token stopToken) { workerLoop(stopToken); });
    }
}

ThreadPool::~ThreadPool() {
    // Explicit for clarity; jthread destructors would do the same.
    for (auto& worker : m_workers) {
        worker.request_stop();
    }
}

std::size_t ThreadPool::pendingTasks() const {
    std::lock_guard lock(m_queueMutex);
    return m_taskQueue.size();
}

void ThreadPool::enqueue(Task task) {
    {
        std::lock_guard lock(m_queueMutex);
        m_taskQueue.push(std::move(task));
    }
    m_queueCv.notify_one();
}

void ThreadPool::workerLoop(const std::stop_token stopToken) {
    while (true) {
        Task task;
        {
            std::unique_lock lock(m_queueMutex);
            // Returns false only if stop was requested and the queue is empty, so remaining tasks are drained before the worker exits.
            if (!m_queueCv.wait(lock, stopToken, [this] { return !m_taskQueue.empty(); })) {
                return;
            }
            task = std::move(m_taskQueue.front());
            m_taskQueue.pop();
        }
        task();
    }
}

} // namespace concurrent
