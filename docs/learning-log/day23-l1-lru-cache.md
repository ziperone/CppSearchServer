# Day 23: L1 本地 LRU 缓存

## 要解决的问题

相同的搜索请求会重复经过分词、倒排索引匹配、TF-IDF 排序和 JSON 序列化。对于热点查询，这些重复计算会占用 WorkerPool，并增加等待时间。

本阶段先实现进程内 L1 缓存。它不依赖 Redis，访问没有网络开销；后续 Redis L2 命中后也会回填 L1。

## 数据结构与复杂度

```text
unordered_map<key, Entry>
    -> O(1) 找到 value、过期时间和 list 位置

list<key>
    -> 表头：最近访问
    -> 表尾：最久未访问，容量满时淘汰
```

`Entry::lru_position` 保存本 key 在 list 中的迭代器。命中时 `list::splice()` 将节点移到表头，不复制 key，因此命中、移动和淘汰均为 O(1)。

## get 的三条路径

1. **不存在**：返回 `std::nullopt`，表示 cache miss；
2. **存在但 TTL 到期**：使用 `steady_clock::now() >= expires_at` 判断，先删 list 节点、再删 map 条目，返回 miss；
3. **命中且有效**：`splice` 移到表头，拷贝 value 返回。

TTL 是固定 TTL：`put()` 时设为 `now + ttl_`，命中只更新 LRU 顺序，不延长到期时间。这样热点旧数据也会定期失效，适合未来文档可更新的场景。`steady_clock` 单调递增，不受系统校时影响。

## put 的两条路径

- key 已存在：更新 value、刷新 TTL、移至表头；
- key 不存在：在 list 表头插入 key，再用 `unordered_map::emplace` 原地构造 Entry；超过容量时删除 list 表尾对应的 map 条目，再弹出表尾节点。

`operator[]` 也能实现插入，但会默认构造 mapped value 后再赋值；`emplace` 直接构造最终 Entry，避免额外构造/复制，也不依赖 Entry 可默认构造。

## 并发边界

`SearchApplication` 未来会被多个 Worker 并发调用，因此 LRU 的 map/list 是共享可变状态。`get()` 和 `put()` 都在函数入口获取同一个 `mutex_`，将“查找 -> 判断 -> 同步修改两个容器”变成不可交错的临界区。

锁只保护缓存内部操作；cache miss 后的检索、排序和 JSON 构造不在锁内执行，否则一个慢查询会把整个缓存变成串行瓶颈。

## 验证

新增 `lru_cache_test`，覆盖：

1. 非法容量和 TTL 被拒绝；
2. 基本命中；
3. 命中后提升最近使用顺序，容量满时淘汰最久未访问项；
4. 更新已有 key 后 value 与 TTL 刷新；
5. TTL 到期后返回 miss 并同步删除；
6. 4 个线程并发写入和读取不同 key。

远程 Linux Release `ctest --test-dir build-ralease --output-on-failure`：**17/17 通过**。

## 当前限制与下一步

- 过期项采用惰性删除：只有被 `get()` 访问或容量淘汰时才回收；
- 同一热点 key 在并发 miss 时可能重复执行检索，后续可用 singleflight/request coalescing 处理缓存击穿；
- L1 仅在本进程有效，重启即丢失，多个服务实例不能共享。

下一步在 `SearchApplication` 的 `/search` 路径缓存 JSON 业务结果，不缓存完整 HTTP 响应；之后安装并接入 Redis 作为 L2 共享缓存。
