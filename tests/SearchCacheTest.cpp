#include "search/SearchCache.h"

#include <cassert>
#include <chrono>
#include <optional>
#include <string>
#include <thread>

namespace {

std::string uniqueKey(std::string_view suffix) {
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    return "cpp-search:test:search-cache:" + std::string(suffix) + ":" +
           std::to_string(tick);
}

search::SearchCache::Config makeConfig() {
    search::SearchCache::Config config;
    config.local_capacity = 2;
    config.local_ttl = std::chrono::seconds(10);
    config.redis_timeout = std::chrono::milliseconds(100);
    config.redis_ttl = std::chrono::seconds(10);
    return config;
}

}  // namespace

int main() {
    search::SearchCache cache(makeConfig());
    const std::string key = uniqueKey("shared-l2");
    const std::string value = "{\"query\":\"epoll\",\"results\":[]}";

    assert(!cache.get(key).has_value());
    cache.put(key, value);
    assert(cache.get(key) == value);

    std::optional<std::string> value_from_another_worker;
    std::thread another_worker([&] {
        value_from_another_worker = cache.get(key);
    });
    another_worker.join();
    assert(value_from_another_worker == value);

    search::SearchCache::Config unavailable_config = makeConfig();
    unavailable_config.redis_port = 6399;
    search::SearchCache unavailable_cache(unavailable_config);
    const std::string unavailable_key = uniqueKey("unavailable");
    unavailable_cache.put(unavailable_key, value);
    assert(unavailable_cache.get(unavailable_key) == value);

    std::optional<std::string> unavailable_from_another_worker;
    std::thread unavailable_worker([&] {
        unavailable_from_another_worker = unavailable_cache.get(unavailable_key);
    });
    unavailable_worker.join();
    assert(!unavailable_from_another_worker.has_value());
}
