#pragma once

#include <sys/epoll.h>

#include <cstdint>
#include <vector>

namespace net {

class Epoller {
public:
    explicit Epoller(int max_events = 1024);
    ~Epoller();

    Epoller(const Epoller&) = delete;
    Epoller& operator=(const Epoller&) = delete;

    void addFd(int fd, std::uint32_t events);
    void modFd(int fd, std::uint32_t events);
    void delFd(int fd);

    std::vector<epoll_event> wait(int timeout_ms = -1);

private:
    void update(int operation, int fd, std::uint32_t events);

    int epoll_fd_;
    std::vector<epoll_event> events_;
};

}  // namespace net

