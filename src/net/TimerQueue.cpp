#include "net/TimerQueue.h"

#include <limits>

namespace net {

void TimerQueue::addTimer(TimePoint expires_at, TimerCallback callback) {
    timers_.push({expires_at, std::move(callback)});
}

int TimerQueue::millisecondsUntilNextTimer(TimePoint now) const {
    if (timers_.empty()) {
        return -1;
    }

    const TimePoint expires_at = timers_.top().expires_at;
    if (expires_at <= now) {
        return 0;
    }

    const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(expires_at - now);
    if (remaining.count() >= std::numeric_limits<int>::max()) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(remaining.count());
}

void TimerQueue::runExpiredTimers(TimePoint now) {
    while (!timers_.empty() && timers_.top().expires_at <= now) {
        Timer timer = timers_.top();
        timers_.pop();
        timer.callback();
    }
}

}  // namespace net
