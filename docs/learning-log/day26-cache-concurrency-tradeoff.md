# Day 26: 缓存并发取舍 - 从全局 LRU 到 Worker-local L1 + Redis L2

## 已验证的事实

当前全局精确 LRU 在热点短检索下将 QPS 从约 36-38 万降至约 28-30 万。命中确实跳过了检索，Worker CPU 从约 76% 降至约 44%，但所有 Worker 都竞争同一个 LruCache mutex，并在每次 hit 中执行 map 查找、TTL 判断和 `list::splice`。

因此这不是缓存失效，而是缓存命中路径的共享开销超过了被省掉的本地检索成本。

## 当前检索为什么便宜

项目在启动时构建内存倒排索引；一次查询不会遍历所有文档。主要成本是查询词 posting list、候选 chunk 数、排序和 JSON 序列化。

- 文档增多但词很稀有，查询仍可能很快；
- 文档增多且词高频、候选集很大，检索成本会增长；
- 后续 MySQL、RAG、向量检索、远程 RPC 等毫秒级下游，会让缓存更有价值。

所以“数据更大后缓存一定快”不是定律，必须以查询分布和下游实际成本压测验证。

## 方案取舍

用户提出每个 Worker 分配子 LRU 的方向正确，属于 worker-local/thread-local cache：

```text
Worker-local L1（无跨 Worker 锁）
    -> Redis L2（多 Worker、多实例共享）
        -> SearchService / MySQL / RAG
```

Worker-local L1 的优势是热点 hit 无需竞争全局 mutex；代价是同一结果可能在多个 Worker 中重复保存、总体命中率略低。这是合理的空间换并发。

不在第一版实现“父 LRU + 子 LRU + 双缓冲 + 新增数据同步队列”，原因：

- 父子 TTL、淘汰、失效版本和同步失败的语义复杂；
- 双缓冲切换需要新的读写所有权协议；
- 父 LRU 最终仍可能成为共享锁竞争点；
- 复杂度和内存成本远超当前小索引的实际收益。

缓存不是权威数据。L1 可以依靠短 TTL 和 `index_version` key 失效，不需要与父级强一致同步；Redis L2 才承担跨 Worker/跨实例的共享层角色。

## 后续验证计划

1. 保留当前全局 LRU 作为“精确 LRU 热点锁竞争”的基准反例；
2. Redis 安装后实现 L2 的 TTL、版本 key、超时与不可用降级；
3. 设计 Worker-local L1，再比较全局 LRU、Worker-local L1、L1+Redis L2；
4. 用热点、混合、冷查询和更大可控语料比较 QPS、P99、命中率、线程 CPU 与内存；
5. 只有当被省掉的下游工作超过缓存热路径开销时，才将缓存作为该负载的性能优化保留。

## 面试表达

“我实现并压测了线程安全精确 LRU，发现小型内存倒排索引的热点查询中，四个 Worker 在全局 LRU mutex 上竞争，缓存命中反而使 QPS 从约 37 万降至约 30 万。Worker CPU 降低证明检索被跳过，但缓存热路径成本更高。因此我没有把缓存包装成虚假优化，而是把架构调整为 worker-local L1 消除热锁、Redis L2 解决多实例共享，并以 TTL 与索引版本而非父子强同步处理失效。”
