#include "net/EventLoop.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    using namespace std::chrono_literals;

    net::EventLoop loop;
    const std::thread::id loop_thread_id = std::this_thread::get_id();
    std::atomic<bool> task_ran {false};
    std::atomic<bool> ran_on_loop_thread {false};

    std::thread worker([&loop, &task_ran, &ran_on_loop_thread, loop_thread_id] {
        std::this_thread::sleep_for(10ms);
        loop.queueInLoop([&loop, &task_ran, &ran_on_loop_thread, loop_thread_id] {
            task_ran.store(true);
            ran_on_loop_thread.store(std::this_thread::get_id() == loop_thread_id);
            loop.quit();
        });
    });

    loop.loop();
    worker.join();

    if (!task_ran.load() || !ran_on_loop_thread.load()) {
        std::cerr << "EventLoop task queue test failed\n";
        return 1;
    }

    std::cout << "EventLoop task queue test passed\n";
    return 0;
}
