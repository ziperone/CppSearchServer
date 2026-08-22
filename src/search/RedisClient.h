#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

struct redisContext;

namespace search {

class RedisClient {
public:
    RedisClient(std::string host,
                int port,
                std::chrono::milliseconds timeout);
    ~RedisClient();

    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

    std::optional<std::string> get(std::string_view key);
    bool setEx(std::string_view key,
               std::string_view value,
               std::chrono::seconds ttl);
    void disconnect();

private:
    bool ensureConnected();

    std::string host_;
    int port_;
    std::chrono::milliseconds timeout_;
    redisContext* context_ = nullptr;
};

}  // namespace search
