#pragma once

#include "net/Epoller.h"

#include <cstdint>
#include <functional>
#include <unordered_map>

namespace net {

class EventLoop {
public:
    using EventCallback = std::function<void(int)>;

    explicit EventLoop(int max_events = 1024);

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    void addReadEvent(int fd, EventCallback callback);
    void removeEvent(int fd);

    void loop();
    void quit();

private:
    void dispatch(int fd, std::uint32_t events);

    bool quit_;
    Epoller epoller_;
    std::unordered_map<int, EventCallback> read_callbacks_;
};

}  // namespace net

