#include "net/EventLoop.h"

#include <sys/epoll.h>

#include <iostream>

namespace net {

EventLoop::EventLoop(int max_events)
    : quit_(false),
      epoller_(max_events),
      read_callbacks_() {}

void EventLoop::addReadEvent(int fd, EventCallback callback) {
    read_callbacks_[fd] = std::move(callback);
    epoller_.addFd(fd, EPOLLIN);
}

void EventLoop::removeEvent(int fd) {
    epoller_.delFd(fd);
    read_callbacks_.erase(fd);
}

void EventLoop::loop() {
    while (!quit_) {
        std::vector<epoll_event> events = epoller_.wait();
        for (const auto& event : events) {
            dispatch(event.data.fd, event.events);
        }
    }
}

void EventLoop::quit() {
    quit_ = true;
}

void EventLoop::dispatch(int fd, std::uint32_t events) {
    if ((events & (EPOLLERR | EPOLLHUP)) != 0) {
        std::cerr << "fd " << fd << " got epoll error/hangup event\n";
        removeEvent(fd);
        return;
    }

    if ((events & EPOLLIN) != 0) {
        auto it = read_callbacks_.find(fd);
        if (it != read_callbacks_.end()) {
            it->second(fd);
        }
    }
}

}  // namespace net

