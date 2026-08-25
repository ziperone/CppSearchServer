# Day 30: Worker-local L1 + Redis L2 接入搜索主链路

## 模块目的

此前 `SearchApplication` 使用一个所有 Worker 共享的精确 LRU。压测已证明：在当前微小内存倒排索引的热点查询中，所有 Worker 竞争同一 mutex，使 QPS 从约 36-38 万降到约 28-30 万。

本阶段将其替换为两级缓存：

```text
当前 Worker 的 L1 LRU（128 条，10 秒 TTL）
    -> Redis L2（60 秒 TTL）
        -> SearchService 本地倒排索引
```

L1 的 128 条容量是每个活跃 Worker 的上限。四个 Worker 的理论容量上限约为 512 条，但热点 key 可重复保存。这是以少量内存和较低总命中率，交换无跨 Worker 热锁的选择。L1 TTL 短于 L2，避免本地副本长期滞留；L2 负责跨 Worker、跨实例复用。

## 关键实现

`SearchCache` 持有不可变配置和唯一 `instance_id_`。`currentThreadState()` 内部维护：

```cpp
static thread_local std::unordered_map<
    std::uint64_t,
    std::unique_ptr<ThreadState>
> states;
```

每个 Worker 线程拥有自己的 map；同一 `SearchCache` 的 `instance_id_` 定位该线程内唯一的 `ThreadState`。`ThreadState` 同时持有 L1 和非线程安全的 hiredis `RedisClient`。使用 `unique_ptr` 是因为 `LruCache` 内部有 mutex，不适合作为 unordered_map 中会随 rehash 移动的值对象；map 移动指针而不移动真实状态。

`SearchCache::get` 是短路读路径：

1. L1 hit：直接返回，完全不访问网络；
2. L1 miss：当前 Worker 使用自己的 Redis connection 执行 GET；
3. L2 hit：回填当前 Worker 的 L1 后返回；
4. L2 miss 或 Redis 故障：返回 `nullopt`，由 `SearchApplication` 调用本地倒排索引。

`SearchCache::put` 先写当前 L1，再 `SET ... EX` 写 Redis L2。L2 写入失败被忽略，因为缓存不是权威数据，搜索结果必须正常返回。

所有缓存 value 是 JSON，不缓存完整 HTTP response；这样 `Connection: close` 与 Keep-Alive 等 HTTP 头仍在每次请求中按当前连接状态生成。

## 测试证明

新增 `search_cache_test`：

1. 主线程 `put` 后立即 `get`，验证同线程 L1 命中；
2. 新建线程调用同一 cache 的 `get`，其 L1 一定为空，能够取得相同值说明命中共享 Redis L2；
3. 使用 6399 未监听端口时，写入线程仍能命中自己的 L1，而新线程 miss，证明 Redis 故障 fail-open 且没有伪造跨线程共享。

`SearchApplication` 已接入 `SearchCache`，保留原有 HTTP、JSON、`Connection: close` 测试。`cacheStats()` 现在只返回**当前调用线程**的 L1 统计，用于现有单线程测试；它不能代表全局命中率。未来导出 `/metrics` 时，应以原子计数器聚合所有 Worker 的 L1/L2 命中、miss、超时与回源次数。

远端 Release 验证：19/19 CTest 通过。

## 当前限制与下阶段

1. Redis 是同步 hiredis 调用，会阻塞 Worker，不会阻塞 EventLoop；需用压测观察 Redis 超时是否导致 Worker 队列和 P99 恶化；
2. 单机本地索引非常轻，Redis 网络开销可能使缓存变慢，不能预设 Redis 一定提升 QPS；
3. 当前索引仅在启动时构建，key 中 `v1` 表示版本。支持文档更新后，必须将 `index_version` 纳入 key，或做主动失效；
4. 若 Redis 连接数或等待时间成为实测瓶颈，再讨论连接池、异步 hiredis 或将计算层迁出单机。

## 面试表达

“全局精确 LRU 在四 Worker 热点压测中产生 mutex 竞争，因此我把 L1 改为 Worker-local，使用 thread-local 状态让每个 Worker 独占 LRU 和 hiredis context；L1 miss 才访问共享 Redis L2，L2 hit 会回填本地。同步 Redis 从不进入 epoll EventLoop，故障则 fail-open 回源本地倒排索引。这个设计优化的是客户端缓存竞争，但 Redis 网络开销是否值得必须通过热、混合、冷查询的 QPS/P99 和 Worker CPU 压测验证。”
