#include "net/EventLoopGroup.h"

#include <chrono>
#include <condition_variable>
#include <exception>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

int main() {
    using namespace std::chrono_literals;

    bool zero_loop_rejected = false;
    try {
        net::EventLoopGroup empty_group(0);
    } catch (const std::logic_error&) {
        zero_loop_rejected = true;
    }

    const std::thread::id caller_thread_id = std::this_thread::get_id();
    std::mutex mutex;
    std::condition_variable tasks_completed;
    int completed_tasks = 0;
    std::thread::id first_io_thread_id;
    std::thread::id second_io_thread_id;
    bool pre_start_rejected = false;
    bool duplicate_start_rejected = false;

    {
        net::EventLoopGroup group(2);

        try {
            group.nextLoop();
        } catch (const std::logic_error&) {
            pre_start_rejected = true;
        }

        group.start();
        net::EventLoop& first = group.nextLoop();
        net::EventLoop& second = group.nextLoop();
        net::EventLoop& third = group.nextLoop();
        net::EventLoop& fourth = group.nextLoop();

        const bool round_robin_correct =
            &first != &second && &first == &third && &second == &fourth;

        try {
            group.start();
        } catch (const std::logic_error&) {
            duplicate_start_rejected = true;
        }

        first.queueInLoop([&] {
            {
                std::lock_guard<std::mutex> lock(mutex);
                first_io_thread_id = std::this_thread::get_id();
                ++completed_tasks;
            }
            tasks_completed.notify_one();
        });

        second.queueInLoop([&] {
            {
                std::lock_guard<std::mutex> lock(mutex);
                second_io_thread_id = std::this_thread::get_id();
                ++completed_tasks;
            }
            tasks_completed.notify_one();
        });

        std::unique_lock<std::mutex> lock(mutex);
        const bool completed = tasks_completed.wait_for(lock, 1s, [&] {
            return completed_tasks == 2;
        });
        if (!completed || !round_robin_correct) {
            std::cerr << "EventLoopGroup round-robin test failed\n";
            return 1;
        }
    }

    const bool tasks_used_distinct_io_threads =
        first_io_thread_id != caller_thread_id &&
        second_io_thread_id != caller_thread_id &&
        first_io_thread_id != second_io_thread_id;

    if (!zero_loop_rejected || !pre_start_rejected || !duplicate_start_rejected ||
        !tasks_used_distinct_io_threads) {
        std::cerr << "EventLoopGroup test failed\n";
        return 1;
    }

    std::cout << "EventLoopGroup test passed\n";
    return 0;
}
