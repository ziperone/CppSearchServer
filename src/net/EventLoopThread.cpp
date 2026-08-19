#include "net/EventLoopThread.h"

#include "net/EventLoop.h"

#include <stdexcept>

namespace net {

EventLoopThread::EventLoopThread()
    : mutex_(),
      state_changed_(),
      loop_(nullptr),
      started_(false),
      thread_() {}
EventLoop& EventLoopThread::startLoop(){
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(started_){
            throw std::logic_error("EventLoopThread has already been started");
        }
        started_ = true;
    }
    thread_ = std::thread(&EventLoopThread::threadMain, this);
    std::unique_lock<std::mutex> lock(mutex_);
    state_changed_.wait(lock,[this]{
        return loop_ != nullptr;
    });
    return *loop_;
}
void EventLoopThread::threadMain(){
    EventLoop loop;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        loop_ = &loop;
    }
    state_changed_.notify_one();
    loop.loop();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        loop_ = nullptr;
    }
}
EventLoopThread::~EventLoopThread() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (loop_ != nullptr) {
            loop_->quit();
        }
    }

    if (thread_.joinable()) {
        thread_.join();
    }
}

}  // namespace net
