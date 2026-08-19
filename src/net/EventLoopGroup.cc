#include "net/EventLoopGroup.h"
#include <cstddef>
#include <memory>
#include <vector>
#include <stdexcept>
namespace net{
EventLoopGroup::EventLoopGroup(std::size_t loop_count)
:threads_(),
loops_(),
next_index_(0),
started_(false){
    if(loop_count == 0){
         throw std::logic_error("EventLoopGroup is empty");
    }
    for(std::size_t i = 0 ; i < loop_count ; ++i ){
        threads_.push_back(std::make_unique<EventLoopThread>());
    }
}


void EventLoopGroup::start(){
    if(started_){
         throw std::logic_error("EventLoopGroup has already been started");
    }
    
    for(const auto& thread : threads_ ){
        EventLoop& loop = thread->startLoop();
        loops_.emplace_back(&loop);
    }
    started_ = true;
}

EventLoop& EventLoopGroup::nextLoop(){
    if(!started_ || loops_.empty()){
        throw std::logic_error("EventLoopGroup eror");
    }
    EventLoop& loop = *loops_[next_index_];
    next_index_ ++;
    if(next_index_ >= loops_.size()){
        next_index_ = 0;
    }
    return loop;

}
}