#include "net/EventLoop.h"
#include "net/EventLoopThread.h"

#include <chrono>
#include <condition_variable>
#include <exception>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

int main() {
    using namespace std::chrono_literals;

    const std::thread::id caller_thread_id = std::this_thread::get_id();
    std::mutex mutex;
    std::condition_variable task_completed;
    bool task_ran = false;
    bool ran_on_io_thread = false;
    bool duplicate_start_rejected = false;

    {
        net::EventLoopThread loop_thread;
        net::EventLoop& loop = loop_thread.startLoop();

        try {
            loop_thread.startLoop();
        } catch (const std::logic_error&) {
            duplicate_start_rejected = true;
        }

        loop.queueInLoop([&] {
            {
                std::lock_guard<std::mutex> lock(mutex);
                task_ran = true;
                ran_on_io_thread = std::this_thread::get_id() != caller_thread_id;
            }
            task_completed.notify_one();
        });

        std::unique_lock<std::mutex> lock(mutex);
        const bool completed = task_completed.wait_for(lock, 1s, [&] {
            return task_ran;
        });
        if (!completed) {
            std::cerr << "queued task did not run on EventLoopThread\n";
            return 1;
        }
    }

    if (!ran_on_io_thread || !duplicate_start_rejected) {
        std::cerr << "EventLoopThread test failed\n";
        return 1;
    }

    std::cout << "EventLoopThread test passed\n";
    return 0;
}
