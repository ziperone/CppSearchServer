#include "search/LruCache.h"

#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

bool hasValue(const std::optional<std::string>& value, const std::string& expected) {
    return value.has_value() && *value == expected;
}

}  // namespace

int main() {
    using namespace std::chrono_literals;

    bool invalid_capacity_rejected = false;
    bool invalid_ttl_rejected = false;
    try {
        search::LruCache invalid_capacity(0, 1s);
    } catch (const std::invalid_argument&) {
        invalid_capacity_rejected = true;
    }
    try {
        search::LruCache invalid_ttl(1, 0ms);
    } catch (const std::invalid_argument&) {
        invalid_ttl_rejected = true;
    }

    search::LruCache cache(2, 1s);
    cache.put("first", "one");
    cache.put("second", "two");
    const bool initial_hit = hasValue(cache.get("first"), "one");

    cache.put("third", "three");
    const bool least_recently_used_evicted =
        !cache.get("second").has_value() &&
        hasValue(cache.get("first"), "one") &&
        hasValue(cache.get("third"), "three");

    cache.put("first", "updated");
    const bool existing_value_updated = hasValue(cache.get("first"), "updated");

    search::LruCache expiring_cache(1, 10ms);
    expiring_cache.put("temporary", "value");
    std::this_thread::sleep_for(20ms);
    const bool expired_entry_removed = !expiring_cache.get("temporary").has_value();

    search::LruCache stats_cache(1, 10ms);
    (void)stats_cache.get("missing");
    stats_cache.put("first", "one");
    (void)stats_cache.get("first");
    stats_cache.put("second", "two");
    std::this_thread::sleep_for(20ms);
    (void)stats_cache.get("second");
    const search::LruCache::Stats stats = stats_cache.stats();
    const bool stats_are_correct =
        stats.hits == 1 && stats.misses == 2 && stats.expirations == 1 && stats.evictions == 1;

    search::LruCache concurrent_cache(64, 1s);
    std::atomic<bool> concurrent_access_succeeded = true;
    std::vector<std::thread> writers;
    for (int thread_index = 0; thread_index < 4; ++thread_index) {
        writers.emplace_back([&concurrent_access_succeeded, &concurrent_cache, thread_index] {
            for (int key_index = 0; key_index < 16; ++key_index) {
                const std::string key =
                    "thread-" + std::to_string(thread_index) + "-key-" + std::to_string(key_index);
                concurrent_cache.put(key, "value");
                if (!concurrent_cache.get(key).has_value()) {
                    concurrent_access_succeeded.store(false);
                }
            }
        });
    }
    for (std::thread& writer : writers) {
        writer.join();
    }

    if (!invalid_capacity_rejected || !invalid_ttl_rejected || !initial_hit ||
        !least_recently_used_evicted || !existing_value_updated || !expired_entry_removed ||
        !stats_are_correct || !concurrent_access_succeeded.load()) {
        std::cerr << "LruCache test failed\n";
        return 1;
    }

    std::cout << "LruCache test passed\n";
    return 0;
}
