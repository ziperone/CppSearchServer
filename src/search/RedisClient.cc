#include "search/RedisClient.h"

#include <hiredis/hiredis.h>

#include <stdexcept>
#include <string_view>
#include <utility>

namespace {

timeval toTimeval(std::chrono::milliseconds timeout) {
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(timeout);
    const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
        timeout - seconds);
    return timeval{static_cast<time_t>(seconds.count()),
                   static_cast<suseconds_t>(microseconds.count())};
}

}  // namespace

namespace search {

RedisClient::RedisClient(std::string host,
                         int port,
                         std::chrono::milliseconds timeout)
    : host_(std::move(host)), port_(port), timeout_(timeout) {
    if (host_.empty()) {
        throw std::invalid_argument("Redis host must not be empty");
    }
    if (port_ <= 0 || port_ > 65535) {
        throw std::invalid_argument("Redis port must be in range 1..65535");
    }
    if (timeout_ <= std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("Redis timeout must be positive");
    }
}

RedisClient::~RedisClient() {
    disconnect();
}

void RedisClient::disconnect() {
    if (context_ != nullptr) {
        redisFree(context_);
        context_ = nullptr;
    }
}

bool RedisClient::ensureConnected() {
    if (context_ != nullptr) {
        return true;
    }

    const timeval timeout = toTimeval(timeout_);
    context_ = redisConnectWithTimeout(host_.c_str(), port_, timeout);
    if (context_ == nullptr || context_->err != 0) {
        disconnect();
        return false;
    }
    if (redisSetTimeout(context_, timeout) != REDIS_OK) {
        disconnect();
        return false;
    }
    return true;
}

std::optional<std::string> RedisClient::get(std::string_view key){
    if(!ensureConnected()){
        return std::nullopt;
    }
    const char* argv[] = {"GET", key.data()};
    const size_t argvlen[] = {3, key.size()};
    auto* reply = static_cast<redisReply*>(
    redisCommandArgv(context_, 2, argv, argvlen));
    if(reply == nullptr){
        disconnect();
        return std::nullopt;
    }
    if(reply->type == REDIS_REPLY_NIL){
        freeReplyObject(reply);
        return std::nullopt;
    }
    if(reply->type == REDIS_REPLY_STRING){
        if (reply->str == nullptr) {
            freeReplyObject(reply);
            disconnect();
            return std::nullopt;
        }
        std::string value(reply->str, reply->len);
        freeReplyObject(reply);
        return value;
    }
    freeReplyObject(reply);
    disconnect();
    return std::nullopt;
}
bool RedisClient::setEx(std::string_view key,
                        std::string_view value,
                        std::chrono::seconds ttl) {
    if(ttl <= std::chrono::seconds::zero()){
        return false;
    }
    if(!ensureConnected()){
        return false;
    }
    std::string ttl_str = std::to_string(ttl.count()); 
    const char* argv[] = {"SET", key.data(),value.data(),"EX",ttl_str.c_str()};
    const size_t argvlen[] = {3, key.size(),value.size(),2,ttl_str.size()};
    auto* reply = static_cast<redisReply*>(
    redisCommandArgv(context_, 5, argv, argvlen));
    if(reply == nullptr){
        disconnect();
        return false;
    }
    const bool success =
        reply->type == REDIS_REPLY_STATUS && reply->str != nullptr &&
        std::string_view(reply->str, reply->len) == "OK";
    freeReplyObject(reply);
    if (!success) {
        disconnect();
    }
    return success;

}

}  // namespace search
