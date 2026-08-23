#include "search/SearchCache.h"

#include "search/LruCache.h"
#include "search/RedisClient.h"

#include <atomic>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace {

std::atomic<std::uint64_t> next_cache_instance_id{1};

}  // namespace

namespace search {

struct SearchCache::ThreadState {
    explicit ThreadState(const Config& config)
        : local_cache(config.local_capacity, config.local_ttl),
          redis(config.redis_host, config.redis_port, config.redis_timeout) {}

    LruCache local_cache;
    RedisClient redis;
};

SearchCache::SearchCache(Config config)
    : config_(std::move(config)),
      instance_id_(next_cache_instance_id.fetch_add(1, std::memory_order_relaxed)) {
    if (config_.local_capacity == 0) {
        throw std::invalid_argument("local cache capacity must be positive");
    }
    if (config_.local_ttl <= std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("local cache TTL must be positive");
    }
    if (config_.redis_ttl <= std::chrono::seconds::zero()) {
        throw std::invalid_argument("Redis cache TTL must be positive");
    }
}
SearchCache::ThreadState& SearchCache::currentThreadState() const{
    static thread_local std::unordered_map<std::uint64_t,std::unique_ptr<ThreadState>> states;
    auto it = states.find(instance_id_);
    if(it != states.end()){
        return *it->second;
    }
    auto new_state = std::make_unique<ThreadState>(config_);
    const auto inserted = states.emplace(instance_id_, std::move(new_state));
    return *inserted.first->second;
}
std::optional<std::string> SearchCache::get(std::string_view key) const{
    const std::string key_copy(key);
    ThreadState& state = currentThreadState();
    auto success_L1 = state.local_cache.get(key_copy);
    if(success_L1){
        return success_L1;
    }
    auto success_L2 = state.redis.get(key_copy);
    if(success_L2){
        state.local_cache.put(key_copy,*success_L2);
    }
    return success_L2;
}

void SearchCache::put(std::string_view key,std::string_view value) const{
    const std::string key_copy(key);
    const std::string value_copy(value);
    ThreadState& state = currentThreadState();
    state.local_cache.put(key_copy,value_copy);
    state.redis.setEx(key_copy, value_copy, config_.redis_ttl);
}

LruCache::Stats SearchCache::currentThreadLocalStats() const {
    return currentThreadState().local_cache.stats();
}

}  // namespace search
