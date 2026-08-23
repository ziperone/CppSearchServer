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

    search::SearchCache::Config local_only_config = makeConfig();
    local_only_config.mode = search::SearchCache::Mode::LocalOnly;
    search::SearchCache local_only_cache(local_only_config);
    const std::string local_only_key = uniqueKey("local-only");
    local_only_cache.put(local_only_key, value);
    assert(local_only_cache.get(local_only_key) == value);

    std::optional<std::string> local_only_from_another_worker;
    std::thread local_only_worker([&] {
        local_only_from_another_worker = local_only_cache.get(local_only_key);
    });
    local_only_worker.join();
    assert(!local_only_from_another_worker.has_value());

    search::SearchCache::Config disabled_config = makeConfig();
    disabled_config.mode = search::SearchCache::Mode::Disabled;
    search::SearchCache disabled_cache(disabled_config);
    const std::string disabled_key = uniqueKey("disabled");
    disabled_cache.put(disabled_key, value);
    assert(!disabled_cache.get(disabled_key).has_value());
}
