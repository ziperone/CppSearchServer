#include "net/EventLoop.h"

#include "net/Channel.h"

#include <sys/epoll.h>

namespace net {

EventLoop::EventLoop(int max_events)
    : quit_(false),
      epoller_(max_events),
      timer_queue_(),
      channels_() {}

void EventLoop::addChannel(const ChannelPtr& channel) {
    channels_[channel->fd()] = channel;
    epoller_.addFd(channel->fd(), channel->events());
}

void EventLoop::updateChannel(const ChannelPtr& channel) {
    epoller_.modFd(channel->fd(), channel->events());
}

void EventLoop::removeChannel(int fd) {
    auto it = channels_.find(fd);
    if (it == channels_.end()) {
        return;
    }
    epoller_.delFd(fd);
    channels_.erase(it);
}

void EventLoop::runAt(TimerQueue::TimePoint expires_at, TimerQueue::TimerCallback callback) {
    timer_queue_.addTimer(expires_at, std::move(callback));
}

void EventLoop::runAfter(std::chrono::milliseconds delay, TimerQueue::TimerCallback callback) {
    runAt(TimerQueue::Clock::now() + delay, std::move(callback));
}

void EventLoop::loop() {
    while (!quit_) {
        const TimerQueue::TimePoint now = TimerQueue::Clock::now();
        const int timeout_ms = timer_queue_.millisecondsUntilNextTimer(now);
        std::vector<epoll_event> events = epoller_.wait(timeout_ms);
        for (const auto& event : events) {
            dispatch(event.data.fd, event.events);
        }
        timer_queue_.runExpiredTimers(TimerQueue::Clock::now());
    }
}

void EventLoop::quit() {
    quit_ = true;
}

void EventLoop::dispatch(int fd, std::uint32_t events) {
    auto it = channels_.find(fd);
    if (it == channels_.end()) {
        return;
    }

    ChannelPtr channel = it->second;
    channel->handleEvent(events);
}

}  // namespace net
