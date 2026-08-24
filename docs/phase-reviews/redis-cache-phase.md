# Redis 缓存阶段验收

## 阶段目标

为 C++ 本地文档搜索服务补充可解释、可验证的 Redis L2 能力，同时回答两个工程问题：

1. Redis 中为什么为搜索结果选择 String，而不是 Hash；
2. 缓存到底是否提高当前服务的端到端性能。

阶段的成功标准不是“接入 Redis”，而是能正确完成读写、过期、故障降级、跨实例共享与性能取舍。

## 已完成能力

- Redis String 缓存完整搜索 JSON，key 为 `search-json:v1:top-k=10:q=<query>`；`SET ... EX` 原子写入 value 与 TTL；
- `RedisClient` 基于 hiredis，使用 `redisCommandArgv` 传递带长度的 key/value，安全处理 JSON、空格和二进制内容；
- `GET` 对 `STRING`、`NIL`、异常 reply 分支进行释放与降级；`redisContext` 失效后断开并惰性重连；
- Worker-local RedisClient：`thread_local` 状态让每个 Worker 独占 L1 和 hiredis context；同步 Redis 只阻塞当前 Worker，绝不阻塞 EventLoop；
- 两级缓存：Worker-local L1 -> Redis L2 -> 本地倒排索引，缓存故障 fail-open；
- 三种运行模式：`none`、`l1`、`l1-redis`，可在同一 Release 二进制下做控制变量压测；
- 真实 Redis 集成测试、跨线程 L2 测试、Redis 不可用降级测试，远端 Release 共 19/19 CTest 通过；
- 双实例脚本验证：实例 A 回源后 `GET +2, SET +1`；实例 B 本地 L1 为空时 `GET +2, SET +0`。额外 GET 来自两个 curl 被不同 Worker 接手，符合 Worker-local L1 的设计。

## 数据模型结论

当前查询缓存为“一条 query key 对应一整份 JSON，整体读写、整体过期”，所以使用 Redis String。

Hash 适合用户资料、文档元数据等字段独立读写的实体，但 Redis 7.0 的 TTL 是整个 Hash key 级别；字段生命周期不同应拆 key，例如长期 `doc:101` Hash 和短生命周期 `doc:101:recent_heat` String。Set 适合标签交集，ZSet 适合热度 TopK，List 适合简单队列，但都不适合当前完整结果缓存。

## 性能验收与默认决策

固定 Release、4 Worker、4 I/O EventLoop、`wrk -t4 -c256 -d30s --latency`、`/search?q=epoll`，每种模式运行 3 次：

| 模式 | 平均 QPS | 平均 P99 |
|---|---:|---:|
| `none` | 375,418 | 0.94 ms |
| `l1` | 296,273 | 1.29 ms |
| `l1-redis` | 283,297 | 1.32 ms |

因此当前默认模式改为 `none`。两级缓存实现保留为显式参数和跨实例能力，而不是默认性能优化。

缓存减少了用户态检索计算，但系统态 CPU 基本持平；当前小型内存倒排索引太轻，缓存的 map/list/mutex/字符串与网络路径成本超过了跳过检索的收益。不能据此推出“Redis 无价值”，只能推出“当前单机热点工作负载不适合默认缓存”。

## 明确不做的事情

- 不用全局 RedisClient + mutex：会让 Worker 在共享 context 上串行；
- 不提前实现连接池：尚未测到 Redis 连接数或等待是瓶颈；
- 不宣称 Redis 提升了当前单机 QPS；
- 不把 Redis 当权威数据源：MySQL 和在线文档版本管理属于下一阶段。

## 中间材料索引

| 主题 | 材料 |
|---|---|
| 原 L1 与反例 | `day23-l1-lru-cache.md`、`day24-l1-cache-integration.md`、`day25-cache-is-not-free.md` |
| 取舍与架构 | `day26-cache-concurrency-tradeoff.md`、`day29-redis-connection-ownership-plan.md` |
| Redis 理论与客户端 | `day27-redis-data-model-and-cache-plan.md`、`day28-redis-client-and-integration-test.md` |
| 两级缓存接入 | `day30-worker-local-l1-redis-l2-integration.md` |
| 压测设计与结果 | `day31-cache-benchmark-design.md`、`benchmarks/cache-mode-hot-query-2026-08-23.md` |

## 阶段结束时的面试表达

“我把 Redis 做成可验证的 L2，而不是无条件优化。搜索结果以 String 保存完整 JSON，写入通过 `SET EX` 原子绑定 TTL；Worker 用 thread-local 独占 hiredis context，Redis 同步调用不进入 EventLoop，故障直接回源。之后我在同一 Release 二进制中加入 `none/l1/l1-redis` 对照开关并各压三轮，发现当前小型内存索引下无缓存平均 37.5 万 QPS，L1 和 L1+Redis 反而更慢，所以默认关闭缓存。双实例实验则证明实例 B 能只读 Redis、不再回源写入。这个结论决定了 Redis 在项目中的定位：跨实例和昂贵下游的共享 L2，不是伪造的单机提速。”
