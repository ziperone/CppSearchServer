#pragma once

#include <chrono>
#include <functional>
#include <queue>
#include <vector>

namespace net {

class TimerQueue {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using TimerCallback = std::function<void()>;

    void addTimer(TimePoint expires_at, TimerCallback callback);

    int millisecondsUntilNextTimer(TimePoint now) const;
    void runExpiredTimers(TimePoint now);

private:
    struct Timer {
        TimePoint expires_at;
        TimerCallback callback;
    };

    struct LaterExpiresFirst {
        bool operator()(const Timer& left, const Timer& right) const {
            return left.expires_at > right.expires_at;
        }
    };

    std::priority_queue<Timer, std::vector<Timer>, LaterExpiresFirst> timers_;
};

}  // namespace net
