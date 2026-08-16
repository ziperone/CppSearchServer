#include "concurrency/WorkerPool.h"

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>

int main() {
    using namespace std::chrono_literals;

    concurrency::WorkerPool pool(2);
    std::mutex mutex;
    std::condition_variable both_started;
    std::condition_variable release_tasks;
    int started_count = 0;
    bool release = false;

    auto blocking_task = [&] {
        std::unique_lock<std::mutex> lock(mutex);
        ++started_count;
        both_started.notify_one();
        release_tasks.wait(lock, [&] {
            return release;
        });
    };

    pool.submit(blocking_task);
    pool.submit(blocking_task);

    bool ran_in_parallel = false;
    {
        std::unique_lock<std::mutex> lock(mutex);
        ran_in_parallel = both_started.wait_for(lock, 1s, [&] {
            return started_count == 2;
        });
        release = true;
    }
    release_tasks.notify_all();

    if (!ran_in_parallel) {
        std::cerr << "WorkerPool test failed: two workers did not execute tasks concurrently\n";
        return 1;
    }

    std::cout << "WorkerPool test passed\n";
    return 0;
}
