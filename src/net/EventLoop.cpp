#include "net/EventLoop.h"

#include "net/Channel.h"

#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void throwSystemError(const std::string& message) {
    throw std::runtime_error(message + ": " + std::strerror(errno));
}

}  // namespace

namespace net {

EventLoop::EventLoop(int max_events)
    : quit_(false),
      epoller_(max_events),
      timer_queue_(),
      channels_(),
      wakeup_fd_(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)),
      wakeup_channel_(),
      pending_tasks_mutex_(),
      pending_tasks_() {
    if (wakeup_fd_ < 0) {
        throwSystemError("eventfd failed");
    }

    wakeup_channel_ = std::make_shared<Channel>(wakeup_fd_);
    wakeup_channel_->setReadCallback([this] {
        handleWakeup();
    });
    wakeup_channel_->setErrorCallback([this] {
        quit();
    });
    wakeup_channel_->enableReading();
    addChannel(wakeup_channel_);
}

EventLoop::~EventLoop() {
    if (wakeup_fd_ < 0) {
        return;
    }

    try {
        removeChannel(wakeup_fd_);
    } catch (...) {
    }
    ::close(wakeup_fd_);
}

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

void EventLoop::queueInLoop(Functor task) {
    {
        std::lock_guard<std::mutex> lock(pending_tasks_mutex_);
        pending_tasks_.push_back(std::move(task));
    }
    wakeup();
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

void EventLoop::wakeup() {
    const std::uint64_t one = 1;
    while (true) {
        const ssize_t written = ::write(wakeup_fd_, &one, sizeof(one));
        if (written == static_cast<ssize_t>(sizeof(one))) {
            return;
        }
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written < 0 && errno == EAGAIN) {
            return;
        }
        throwSystemError("eventfd write failed");
    }
}

void EventLoop::handleWakeup() {
    std::uint64_t counter {};
    while (true) {
        const ssize_t read_count = ::read(wakeup_fd_, &counter, sizeof(counter));
        if (read_count == static_cast<ssize_t>(sizeof(counter))) {
            break;
        }
        if (read_count < 0 && errno == EINTR) {
            continue;
        }
        if (read_count < 0 && errno == EAGAIN) {
            return;
        }
        throwSystemError("eventfd read failed");
    }

    std::vector<Functor> tasks;
    {
        std::lock_guard<std::mutex> lock(pending_tasks_mutex_);
        tasks.swap(pending_tasks_);
    }

    for (auto& task : tasks) {
        task();
    }
}

}  // namespace net
