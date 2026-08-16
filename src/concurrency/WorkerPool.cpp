#include "concurrency/WorkerPool.h"

#include <iostream>
#include <stdexcept>
#include <utility>

namespace concurrency {

WorkerPool::WorkerPool(std::size_t worker_count)
    : mutex_(),
      task_available_(),
      tasks_(),
      stopping_(false),
      workers_() {
    if (worker_count == 0) {
        throw std::invalid_argument("worker_count must be positive");
    }

    workers_.reserve(worker_count);
    for (std::size_t index = 0; index < worker_count; ++index) {
        workers_.emplace_back([this] {
            workerLoop();
        });
    }
}

WorkerPool::~WorkerPool() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    task_available_.notify_all();

    for (std::thread& worker : workers_) {
        worker.join();
    }
}

void WorkerPool::submit(Task task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) {
            throw std::runtime_error("cannot submit a task to a stopping WorkerPool");
        }
        tasks_.push(std::move(task));
    }
    task_available_.notify_one();
}

void WorkerPool::workerLoop() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            task_available_.wait(lock, [this] {
                return stopping_ || !tasks_.empty();
            });

            if (stopping_ && tasks_.empty()) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
        }

        try {
            task();
        } catch (const std::exception& error) {
            std::cerr << "WorkerPool task failed: " << error.what() << '\n';
        } catch (...) {
            std::cerr << "WorkerPool task failed with an unknown exception\n";
        }
    }
}

}  // namespace concurrency
