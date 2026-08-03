#pragma once

#include "net/Epoller.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace net {

class Channel;

class EventLoop {
public:
    using ChannelPtr = std::shared_ptr<Channel>;

    explicit EventLoop(int max_events = 1024);

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    void addChannel(const ChannelPtr& channel);
    void updateChannel(const ChannelPtr& channel);
    void removeChannel(int fd);

    void loop();
    void quit();

private:
    void dispatch(int fd, std::uint32_t events);

    bool quit_;
    Epoller epoller_;
    std::unordered_map<int, ChannelPtr> channels_;
};

}  // namespace net
