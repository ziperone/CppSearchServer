#include "search/LruCache.h"

#include <stdexcept>
#include <utility>

namespace search {

LruCache::LruCache(std::size_t capacity, std::chrono::milliseconds ttl)
    : capacity_(capacity), ttl_(ttl) {
    if (capacity_ == 0) {
        throw std::invalid_argument("LRU cache capacity must be positive");
    }
    if (ttl_ <= std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("LRU cache TTL must be positive");
    }
}

std::optional<std::string> LruCache::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto entry_it = entries_.find(key);
    if (entry_it == entries_.end()) {
        ++stats_.misses;
        return std::nullopt;
    }

    Entry& entry = entry_it->second;
    if (std::chrono::steady_clock::now() >= entry.expires_at) {
        lru_keys_.erase(entry.lru_position);
        entries_.erase(entry_it);
        ++stats_.misses;
        ++stats_.expirations;
        return std::nullopt;
    }

    lru_keys_.splice(lru_keys_.begin(), lru_keys_, entry.lru_position);
    ++stats_.hits;
    return entry.value;
}

void LruCache::put(std::string key, std::string value) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto entry_it = entries_.find(key);
    const auto now = std::chrono::steady_clock::now();
    if (entry_it != entries_.end()) {
        Entry& entry = entry_it->second;
        entry.value = std::move(value);
        entry.expires_at = now + ttl_;
        lru_keys_.splice(lru_keys_.begin(), lru_keys_, entry.lru_position);
        return;
    }
    lru_keys_.push_front(key);
    entries_.emplace(
        std::move(key),
        Entry{std::move(value), lru_keys_.begin(), now + ttl_});
    if (entries_.size() > capacity_) {
        const std::string& oldest_key = lru_keys_.back();
        entries_.erase(oldest_key);
        lru_keys_.pop_back();
        ++stats_.evictions;
    }
}

LruCache::Stats LruCache::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

}  // namespace search
