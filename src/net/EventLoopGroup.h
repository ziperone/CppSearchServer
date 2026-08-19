#pragma once
#include "net/EventLoop.h"
#include "net/EventLoopThread.h"
#include <cstddef>
#include <memory>
#include <vector>
namespace net{
class EventLoopGroup {
public:
    explicit EventLoopGroup(std::size_t loop_count);

    void start();
    EventLoop& nextLoop();

private:
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
    std::size_t next_index_;
    bool started_;
};
}