# Day 24: 将 L1 缓存接入真实搜索路径

## 接入位置

缓存位于 `SearchApplication::handleRequest()` 的 `/search` 分支：

```text
HTTP 解析得到 q
    -> makeSearchCacheKey(q)
    -> L1 get
        -> hit：拿缓存 JSON
        -> miss：SearchService 检索 + toJson + L1 put
    -> http::okJson(JSON, close_after_response)
```

缓存 value 是 JSON 业务结果，而不是完整 HTTP 响应。`Connection: close` 与 `keep-alive` 是当前请求/连接的属性，必须在每一次请求末尾重新调用 `http::okJson` 构造响应。

## Cache Key

```text
search-json:v1:top-k=<kSearchTopK>:q=<原始已解码 query>
```

- `top-k` 影响返回结果，必须进入 key；
- `v1` 是缓存格式和检索策略版本。将来改变 JSON 格式、排序逻辑或索引语义时可升级版本，避免复用旧结果；
- 当前 key 使用原始已解码 query，因此 `Epoll` 与 `epoll` 虽然可能得到相同检索结果，也会独立缓存。这是第一版为了保证响应中 `query` 字段准确、逻辑直观所做的取舍；
- Redis L2 阶段会进一步处理跨进程 key 编码与文档版本失效。

## 为什么使用 mutable

`handleRequest()` 保持 `const`：从业务语义看，它不会修改文档块、倒排索引或搜索规则。`search_cache_` 是性能状态，内部自行用 mutex 保证线程安全，因此以 `mutable` 成员允许多个 Worker 在 const 查询中更新 LRU 顺序和统计数据。

## 可观测性

`LruCache::Stats` 提供：

- `hits`：有效缓存命中；
- `misses`：不存在或已过期；
- `expirations`：TTL 到期后被惰性删除；
- `evictions`：容量满导致的 LRU 淘汰。

Stats 和 map/list 使用同一个 mutex，确保快照与更新没有数据竞争。当前通过 `SearchApplication::cacheStats()` 暴露给测试；后续可接入 `/metrics` 或 Prometheus 采集。

## 验证

`SearchApplicationTest` 的请求顺序：

1. `epoll`：第一次查询，cache miss；
2. `epoll reactor`：miss；
3. `not_exist_term`：miss，也缓存空结果；
4. `epoll + Connection: close`：命中第一次的 JSON；
5. 断言 `hits == 1`、`misses == 3`，并断言本次响应仍含 `Connection: close`。

这同时验证了缓存命中、空结果缓存和“不能缓存连接响应头”的边界。

远程 Linux Release `ctest --test-dir build-ralease --output-on-failure`：**17/17 通过**。

## 下一步

1. 固定当前多 Reactor 最佳线程配置，用热点/冷查询比较 L1 命中前后的 QPS、P99 与 CPU；
2. 安装 Redis/hiredis，接入 L2 共享缓存，L2 hit 后回填 L1；
3. 处理 Redis 不可用降级、TTL、缓存穿透和多实例共享问题。
