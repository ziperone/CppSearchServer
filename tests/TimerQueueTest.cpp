#include "net/TimerQueue.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

bool expect(bool condition, std::string_view message) {
    if (condition) {
        return true;
    }

    std::cerr << "Test failed: " << message << '\n';
    return false;
}

}  // namespace

int main() {
    using namespace std::chrono_literals;

    const net::TimerQueue::TimePoint base {};
    net::TimerQueue timers;
    std::vector<char> executed;

    bool passed = true;
    passed &= expect(timers.millisecondsUntilNextTimer(base) == -1,
                     "an empty queue should allow an infinite epoll wait");

    timers.addTimer(base + 10ms, [&executed] {
        executed.push_back('B');
    });
    timers.addTimer(base + 5ms, [&executed] {
        executed.push_back('A');
    });
    timers.addTimer(base + 5ms, [&executed] {
        executed.push_back('C');
    });

    passed &= expect(timers.millisecondsUntilNextTimer(base) == 5,
                     "the queue should expose the earliest expiry");

    timers.runExpiredTimers(base + 4ms);
    passed &= expect(executed.empty(), "a timer must not run before its expiry");
    passed &= expect(timers.millisecondsUntilNextTimer(base + 4ms) == 1,
                     "the remaining wait should shrink as time advances");

    timers.runExpiredTimers(base + 5ms);
    passed &= expect(executed.size() == 2,
                     "all timers due in the same loop iteration should run");
    passed &= expect(std::find(executed.begin(), executed.end(), 'A') != executed.end()
                         && std::find(executed.begin(), executed.end(), 'C') != executed.end(),
                     "both earliest timers should run before the later timer");
    passed &= expect(timers.millisecondsUntilNextTimer(base + 5ms) == 5,
                     "a later timer should remain queued after earlier timers run");

    timers.runExpiredTimers(base + 10ms);
    passed &= expect(executed.size() == 3 && executed.back() == 'B',
                     "the later timer should run only when it expires");
    passed &= expect(timers.millisecondsUntilNextTimer(base + 10ms) == -1,
                     "the queue should be empty after all timers run");

    if (passed) {
        std::cout << "TimerQueue test passed\n";
        return 0;
    }
    return 1;
}
