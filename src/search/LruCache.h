#pragma once
#include <list>
#include <unordered_map>
#include <string>
#include <cstddef>
#include <optional>
#include <chrono>
#include <cstdint>
#include <mutex>
namespace search {

class LruCache {
public:
    struct Stats {
        std::uint64_t hits = 0;
        std::uint64_t misses = 0;
        std::uint64_t expirations = 0;
        std::uint64_t evictions = 0;
    };

    LruCache(std::size_t capacity,
             std::chrono::milliseconds ttl);

    std::optional<std::string> get(const std::string& key);
    void put(std::string key, std::string value);
    Stats stats() const;

private:
    struct Entry {
        std::string value;
        std::list<std::string>::iterator lru_position;
        std::chrono::steady_clock::time_point expires_at;
    };

    std::size_t capacity_;
    std::chrono::milliseconds ttl_;
    std::list<std::string> lru_keys_;
    std::unordered_map<std::string, Entry> entries_;
    Stats stats_;
    mutable std::mutex mutex_;
};
}
