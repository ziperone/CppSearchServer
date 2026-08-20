#pragma once
#include <list>
#include <unordered_map>
#include <string>
#include <cstddef>
#include <optional>
#include <chrono>
#include <mutex>
namespace search {

class LruCache {
public:
    LruCache(std::size_t capacity,
             std::chrono::milliseconds ttl);

    std::optional<std::string> get(const std::string& key);
    void put(std::string key, std::string value);

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
    std::mutex mutex_;
};
}