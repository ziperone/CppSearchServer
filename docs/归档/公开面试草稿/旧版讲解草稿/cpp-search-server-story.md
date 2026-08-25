# CppSearchServer 项目讲述骨架

## 一句话定位

这是一个面向个人求职知识库的 C++ 高并发本地文档检索服务：它将 Markdown/TXT 分块并建立倒排索引，通过多 Reactor + WorkerPool 提供 HTTP 搜索，并用压测驱动并发与缓存取舍。

## 90 秒主线

“我做这个项目的出发点是把简历、项目记录、面试经验和理论笔记做成可低延迟检索的本地知识库。服务启动时把 Markdown/TXT 按标题切成 chunk，建立 term 到 chunk 的倒排索引；请求 `/search?q=...` 后按多词覆盖度和词频计算 TopK，返回 JSON。

网络层一开始是阻塞式 socket 服务，单个慢连接会卡住后续请求。我把它演进为非阻塞 epoll Reactor：Channel 封装 fd 与回调，EventLoop 负责就绪事件分发，TcpConnection 管理输入/输出缓冲区、半关闭、Keep-Alive 和空闲超时。之后业务检索交给 WorkerPool，结果通过 eventfd 回到原 I/O EventLoop，保证 Worker 不直接操作 fd 或 output buffer。

压测发现单 I/O Reactor 会成为瓶颈，于是演进为主 Acceptor 加多个子 EventLoop 的多 Reactor 模型；Worker 数和 I/O Loop 数的对照实验展示了瓶颈会在检索计算与 I/O 吞吐之间迁移。

最后我实现了 Worker-local L1 和 Redis L2，并不是直接宣传缓存优化，而是加入 `none/l1/l1-redis` 对照开关压测。当前小型内存索引下无缓存反而最快，平均约 37.5 万 QPS、P99 0.94ms；我因此默认关闭缓存，但用双实例实验验证 Redis 能承担跨实例共享 L2。这个项目让我形成的结论是：高性能设计不能只堆技术，必须用请求路径、资源所有权和数据证明每个取舍。”

## 讲述顺序

1. **真实问题**：个人知识库文档多、信息碎、希望本地低延迟检索；
2. **检索功能**：分块、倒排索引、多词召回排序、HTTP JSON；
3. **连接生命周期**：非阻塞 socket、epoll、Channel、Buffer、Keep-Alive、Timer；
4. **并发边界**：EventLoop 只做 I/O，Worker 只做业务，eventfd 把结果交回原连接所属 loop；
5. **性能演进**：阻塞 -> 单 Reactor -> WorkerPool -> 多 Reactor，说明每次瓶颈与数据；
6. **缓存反例**：全局 LRU 锁竞争、Worker-local L1、Redis L2、三模式压测结论；
7. **当前边界与下一阶段**：Redis 不做权威数据，MySQL 与文档版本管理后续实现。

## 可量化证据

| 证据 | 可讲的结论 |
|---|---|
| 单 Reactor / 多 Reactor 压测 | I/O 线程会成为独立瓶颈，增加 Worker 不一定提升 QPS |
| Worker 数对照 | 计算与 I/O 会相互迁移，线程数不能无限叠加 |
| 全局 LRU 热点测试 | 精确 LRU 的共享 mutex 可能使缓存命中仍然变慢 |
| 三缓存模式、每种三轮 | `none` 37.5 万 QPS 优于 `l1` 29.6 万和 `l1-redis` 28.3 万 |
| 双实例 Redis 验证 | L2 解决的是跨实例共享，而不是单机热点必然加速 |
| 20/20 CTest | 网络、HTTP、检索、定时器、异步请求、Redis 和请求耗时指标都有回归覆盖 |

## 高频追问的答题方向

| 追问 | 回答核心 |
|---|---|
| 为什么 EventLoop 不直接检索？ | 检索可能耗时，阻塞 I/O 会拖慢全部 fd；Worker 完成后通过 eventfd 回原 loop |
| 为什么 Worker 不直接 send？ | output buffer、Channel、timer 都归属 I/O loop；跨线程操作会产生竞争与响应乱序 |
| 为什么 Redis String 而不是 Hash？ | 一个 query 对应完整 JSON，整体读写、整体 TTL；Hash 适合字段独立读写 |
| 为什么不共享一个 RedisClient？ | hiredis context 非线程安全，共享需 mutex，会形成新的串行瓶颈 |
| 为什么缓存变慢仍保留？ | 当前轻索引不适合；Redis 对跨实例、冷启动、MySQL/RAG 下游仍有合理价值 |
| 下一步如何扩展？ | MySQL 管文档元数据和版本，Redis 缓存版本化查询结果，缓存失效由 index_version 控制 |

## 不要夸大的表述

- 不说“Redis 将 QPS 提升了”；当前数据相反；
- 不说“多线程一定更快”；要说明 Worker、I/O、锁和上下文切换的边界；
- 不说“项目已经分布式”；当前完成的是多实例 Redis 共享验证，MySQL 和真正分布式部署属于下一阶段；
- 不背所有函数细节，重点说明 fd 所有权、异步回调顺序和数据驱动取舍。
