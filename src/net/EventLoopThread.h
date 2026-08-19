#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

namespace net {

class EventLoop;

class EventLoopThread {
public:
    EventLoopThread();
    ~EventLoopThread();

    EventLoopThread(const EventLoopThread&) = delete;
    EventLoopThread& operator=(const EventLoopThread&) = delete;

    EventLoop& startLoop();

private:
    void threadMain();

    std::mutex mutex_;
    std::condition_variable state_changed_;
    EventLoop* loop_;
    bool started_;
    std::thread thread_;
};

}  // namespace net