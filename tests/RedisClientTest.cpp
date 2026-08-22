#include "search/RedisClient.h"

#include <cassert>
#include <chrono>
#include <string>
#include <thread>

int main() {
    using namespace std::chrono_literals;

    search::RedisClient client("127.0.0.1", 6379, 100ms);
    const std::string key = "cpp-search:test:redis-client:ttl";

    assert(!client.setEx(key, "invalid-ttl", 0s));
    assert(client.setEx(key, "epoll reactor", 1s));

    const auto hit = client.get(key);
    assert(hit.has_value());
    assert(*hit == "epoll reactor");

    std::this_thread::sleep_for(1200ms);
    assert(!client.get(key).has_value());

    search::RedisClient unavailable("127.0.0.1", 6399, 20ms);
    assert(!unavailable.get("cpp-search:test:redis-client:unavailable").has_value());
    assert(!unavailable.setEx("cpp-search:test:redis-client:unavailable", "value", 1s));
}
