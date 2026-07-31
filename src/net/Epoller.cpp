#include "net/Epoller.h"

#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

void throwSystemError(const std::string& message) {
    throw std::runtime_error(message + ": " + std::strerror(errno));
}

}  // namespace

namespace net {

Epoller::Epoller(int max_events)
    : epoll_fd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_() {
    if (epoll_fd_ < 0) {
        throwSystemError("epoll_create1 failed");
    }
    if (max_events <= 0) {
        ::close(epoll_fd_);
        throw std::invalid_argument("max_events must be positive");
    }
    events_.resize(static_cast<std::size_t>(max_events));
}

Epoller::~Epoller() {
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
    }
}

void Epoller::addFd(int fd, std::uint32_t events) {
    update(EPOLL_CTL_ADD, fd, events);
}

void Epoller::modFd(int fd, std::uint32_t events) {
    update(EPOLL_CTL_MOD, fd, events);
}

void Epoller::delFd(int fd) {
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        throwSystemError("epoll_ctl DEL failed");
    }
}

std::vector<epoll_event> Epoller::wait(int timeout_ms) {
    int n = ::epoll_wait(epoll_fd_, events_.data(), static_cast<int>(events_.size()), timeout_ms);
    if (n < 0) {
        if (errno == EINTR) {
            return {};
        }
        throwSystemError("epoll_wait failed");
    }

    return std::vector<epoll_event>(events_.begin(), events_.begin() + n);
}

void Epoller::update(int operation, int fd, std::uint32_t events) {
    epoll_event event {};
    event.events = events;
    event.data.fd = fd;

    if (::epoll_ctl(epoll_fd_, operation, fd, &event) < 0) {
        throwSystemError("epoll_ctl update failed");
    }
}

}  // namespace net
