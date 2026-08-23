#pragma once

#include "search/LruCache.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace search {

class SearchCache {
public:
    struct Config {
        std::size_t local_capacity = 128;
        std::chrono::milliseconds local_ttl{10000};
        std::string redis_host = "127.0.0.1";
        int redis_port = 6379;
        std::chrono::milliseconds redis_timeout{50};
        std::chrono::seconds redis_ttl{60};
    };

    explicit SearchCache(Config config);

    std::optional<std::string> get(std::string_view key) const;
    void put(std::string_view key, std::string_view value) const;
    LruCache::Stats currentThreadLocalStats() const;

private:
    struct ThreadState;

    ThreadState& currentThreadState() const;

    Config config_;
    std::uint64_t instance_id_;
};

}  // namespace search
