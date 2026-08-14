#include "net/EventLoop.h"

#include <chrono>
#include <iostream>

int main() {
    using namespace std::chrono_literals;

    net::EventLoop loop;
    bool callback_ran = false;
    const auto started_at = net::TimerQueue::Clock::now();

    loop.runAfter(5ms, [&loop, &callback_ran] {
        callback_ran = true;
        loop.quit();
    });
    loop.loop();

    const auto elapsed = net::TimerQueue::Clock::now() - started_at;
    if (!callback_ran || elapsed < 5ms) {
        std::cerr << "EventLoop timer test failed\n";
        return 1;
    }

    std::cout << "EventLoop timer test passed\n";
    return 0;
}
