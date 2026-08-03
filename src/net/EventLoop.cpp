#include "net/EventLoop.h"

#include "net/Channel.h"

#include <sys/epoll.h>

namespace net {

EventLoop::EventLoop(int max_events)
    : quit_(false),
      epoller_(max_events),
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
    auto it = channels_.find(fd);
    if (it == channels_.end()) {
        return;
    }

    ChannelPtr channel = it->second;
    channel->handleEvent(events);
}

}  // namespace net
