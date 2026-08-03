#include "net/Channel.h"

#include <sys/epoll.h>

#include <utility>

namespace net {

Channel::Channel(int fd)
    : fd_(fd),
      events_(0),
      read_callback_(),
      error_callback_() {}

int Channel::fd() const {
    return fd_;
}

std::uint32_t Channel::events() const {
    return events_;
}

void Channel::enableReading() {
    events_ |= EPOLLIN;
}

void Channel::disableAll() {
    events_ = 0;
}

void Channel::setReadCallback(EventCallback callback) {
    read_callback_ = std::move(callback);
}

void Channel::setErrorCallback(EventCallback callback) {
    error_callback_ = std::move(callback);
}

void Channel::handleEvent(std::uint32_t revents) {
    if ((revents & (EPOLLERR | EPOLLHUP)) != 0){
        if (error_callback_){
            error_callback_();
        }
        return;
    }
    if ((revents & EPOLLIN) != 0){
        if(read_callback_){
            read_callback_();
        }
    }
}
}  // namespace net
