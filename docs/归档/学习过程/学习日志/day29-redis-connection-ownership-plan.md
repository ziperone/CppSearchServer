# Day 29: Redis 连接所有权与两级缓存接入计划

## 先区分三类对象

1. **HTTP 客户端连接**：浏览器到 C++ 服务的 socket，由所属 I/O EventLoop 和 `TcpConnection` 管理；
2. **Worker 线程**：从 `WorkerPool` 的进程内队列取搜索任务，执行 HTTP 解析后的业务计算；
3. **Redis 连接**：Worker 到 Redis 的独立 TCP 连接，对应一个 hiredis `redisContext`。

三者不是一一对应。一个 Worker 会在一段时间内顺序处理很多 HTTP 请求；一个 HTTP 连接上的请求仍按当前状态机串行响应；多个 Worker 则可同时向 Redis 发起各自的命令。

## 选择：Worker-local RedisClient

当前 `WorkerPool::Task` 是 `std::function<void()>`，没有显式传递 Worker 编号。最小且符合现有结构的方案是：在缓存访问类中使用 `thread_local` 状态。

```text
Worker 线程 0 -> thread_local RedisClient 0 -> Redis TCP 连接 0
Worker 线程 1 -> thread_local RedisClient 1 -> Redis TCP 连接 1
Worker 线程 2 -> thread_local RedisClient 2 -> Redis TCP 连接 2
Worker 线程 3 -> thread_local RedisClient 3 -> Redis TCP 连接 3
```

连接是惰性创建的：某个 Worker 第一次真正访问 L2 时才创建自己的 `RedisClient`；以后该线程上的任务复用同一 `redisContext`。线程结束时 `thread_local` 对象析构并释放连接。

这不是“一个 Redis 线程”，也不是每个 HTTP 连接分配一条 Redis 连接。Redis 服务端接收多条客户端 TCP 连接；命令执行主要仍在 Redis 的主执行线程中串行完成，但不同 Worker 的网络等待不会在客户端共享同一个 context 或 mutex 上排队。每条 Redis 命令本身是原子的，`SET ... EX ...` 在服务端作为一个命令执行。

## 一次请求的完整流程

```text
HTTP fd 就绪
 -> 所属 I/O EventLoop / TcpConnection 读取完整请求
 -> WorkerPool 队列
 -> 某个 Worker 线程执行 SearchApplication
 -> 该 Worker 的 L1 查找
 -> 未命中：该 Worker 的 RedisClient GET L2
 -> L2 命中：生成 HTTP JSON 响应
 -> L2 未命中/故障：本地倒排索引 search
 -> Worker 的 RedisClient SET EX 写 L2（失败不影响结果）
 -> ResponseCallback 经 eventfd 回到原 I/O EventLoop
 -> TcpConnection 按连接顺序发送响应
```

同步 hiredis 命令只会阻塞当前 Worker，不会阻塞 EventLoop。若所有 Worker 都在等待 Redis，新的任务会在现有 WorkerPool 队列等待；这正是后续压测需要观察 Redis 超时、Worker 利用率、队列等待和 P99 的原因。

## 最终缓存层级

```text
Worker-local L1: 无跨 Worker 锁，短 TTL，小容量
    -> Redis L2: 跨 Worker、跨服务实例共享，较长 TTL
        -> 本地倒排索引 / 后续 MySQL 或 RAG
```

缓存的 value 都是搜索 JSON，而不是完整 HTTP response，以便连接的 `Connection: close/keep-alive` 语义仍由 `SearchApplication` 在每次请求时正确生成。

Redis L2 key 使用已存在的 `search-json:v1:top-k=10:q=<query>` 格式。当前索引只在进程启动时构建，`v1` 足以标识索引版本；后续支持文档增量更新时，key 必须加入 `index_version` 来失效旧结果。

## 方案取舍

| 方案 | 优点 | 代价 | 结论 |
|---|---|---|---|
| 全局 RedisClient + mutex | 改动最少 | 所有 Worker 在一把锁上串行，可能重演全局 LRU 问题 | 不采用 |
| Worker-local RedisClient | 无跨 Worker context 竞争；复用长连接；贴合当前 WorkerPool | 连接数约等于活跃 Worker 数；每个 Worker 串行处理自己的任务 | 当前采用 |
| Redis 连接池 | 连接数可独立于 Worker 数；可限制 Redis 连接 | 需要 mutex、条件变量、RAII lease，连接满时 Worker 等待 | 后续扩展 |
| 单独 Redis 线程 + 队列 | 可以集中批处理 | 形成新的单点队列，需要异步回复关联与复杂状态机 | 当前规模不采用 |

## 实现顺序

1. 新建 `SearchCache`，持有配置但不共享 hiredis context；通过 `thread_local` 为当前线程取得 RedisClient；
2. 将全局 `search_cache_` 的热点路径替换为 Worker-local L1，保留旧压测结论作为历史基准；
3. 接入 L2：L1 miss 后 GET，L2 miss 后搜索并 SET EX；Redis 异常 fail-open；
4. 增加测试：跨 cache/thread 的 L2 共享、Redis 不可用、本地 L1 不互相竞争；
5. 在热/混合/冷查询下压测，并与全局 LRU 基准对比 QPS、P99、命中率、Worker CPU；
6. 只有在 Worker 因 Redis 等待或连接数限制成为实测瓶颈时，再实现连接池或异步 Redis。

## 面试表达

“RedisClient 封装的是一条非线程安全 hiredis TCP context。我没有用全局 mutex 共享它，而是在 Worker 侧使用 thread-local 连接：每个活跃 Worker 首次访问 L2 时建立连接，后续复用。HTTP I/O 线程绝不执行同步 Redis 命令；Redis miss 或错误都回源搜索。这样解决的是客户端连接的所有权和锁竞争，不宣称 Redis 服务端会因多连接而把单条命令并行执行；是否值得增加连接池，必须由 Redis 等待、Worker 饱和和 P99 的压测数据决定。”
