#pragma once

#include "net/Epoller.h"
#include "net/TimerQueue.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <atomic>

namespace net {

class Channel;

class EventLoop {
public:
    using ChannelPtr = std::shared_ptr<Channel>;
    using Functor = std::function<void()>;

    explicit EventLoop(int max_events = 1024);
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    void addChannel(const ChannelPtr& channel);
    void updateChannel(const ChannelPtr& channel);
    void removeChannel(int fd);

    void runAt(TimerQueue::TimePoint expires_at, TimerQueue::TimerCallback callback);
    void runAfter(std::chrono::milliseconds delay, TimerQueue::TimerCallback callback);
    void queueInLoop(Functor task);

    void loop();
    void quit();

private:
    void dispatch(int fd, std::uint32_t events);
    void wakeup();
    void handleWakeup();

    std::atomic<bool> quit_;
    Epoller epoller_;
    TimerQueue timer_queue_;
    std::unordered_map<int, ChannelPtr> channels_;
    int wakeup_fd_;
    ChannelPtr wakeup_channel_;
    std::mutex pending_tasks_mutex_;
    std::vector<Functor> pending_tasks_;
};

}  // namespace net
