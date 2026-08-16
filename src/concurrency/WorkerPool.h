#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace concurrency {

class WorkerPool {
public:
    using Task = std::function<void()>;

    explicit WorkerPool(std::size_t worker_count);
    ~WorkerPool();

    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    void submit(Task task);

private:
    void workerLoop();

    std::mutex mutex_;
    std::condition_variable task_available_;
    std::queue<Task> tasks_;
    bool stopping_;
    std::vector<std::thread> workers_;
};

}  // namespace concurrency
