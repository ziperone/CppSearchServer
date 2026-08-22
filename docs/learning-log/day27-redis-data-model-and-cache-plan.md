# Day 27: Redis 数据模型与二级缓存学习计划

## 本阶段原则

Redis 的学习不只停留在调用 API。每次使用一种数据结构，都要能回答：

1. 业务数据的访问模式是什么；
2. 为什么选择该数据类型，而不是其他类型；
3. 操作的时间复杂度、内存代价和并发边界是什么；
4. Redis 不可用、数据过期或索引更新时如何保证服务正确性。

每个小阶段完成后，使用“场景 -> 数据模型 -> 命令 -> 复杂度 -> 失败处理”的方式复述。

## 先纠正一个容易混淆的回答

“检索缓存是键值对，所以使用 map”不够准确。`map` 是 C++ 容器；Redis 不是把 C++ `map` 直接作为一个通用存储类型。Redis 为一个 key 选择具体的 **value 数据类型**，常用的有 String、Hash、List、Set、ZSet。

对于当前场景，缓存条目是：

```text
key   = search-json:v1:top-k=10:q=epoll
value = 一整个搜索结果 JSON
TTL   = 60 秒
```

应使用 Redis **String**：一个 key 对应一个完整序列化结果，读取时只需要整体命中或整体未命中，`GET`/`SETEX` 的平均时间复杂度均为 O(1)。这里的 String 是二进制安全的字节串，不只是“普通文字”。JSON 正是其中的 value。

Redis **Hash** 也能按 field 查找，`HGET`/`HSET` 平均 O(1)，但它适用于“一个实体有多个可独立读取、更新的字段”，例如：

```text
user:1001 -> { name: "Ruiyun", city: "Beijing", score: "95" }
```

若把检索结果硬拆进 Hash，读取一个完整结果仍需取回多个 field，并不会比一个 String 更自然；同时 TTL 只能设置在整个 Redis key 上，不能单独设置 Hash 的某个 field。故当前不选 Hash。

## Redis 与本项目的角色

```text
Worker-local L1（后续，无跨 Worker 锁）
    -> Redis L2（跨 Worker、跨服务实例共享）
        -> 本地 SearchService / 后续 MySQL 或 RAG
```

Redis 是加速层而不是权威数据源。缓存 miss 或 Redis 故障时，Worker 必须继续调用 `SearchApplication` 计算结果并返回；不能阻塞 EventLoop，也不能让 Redis 故障直接变成 HTTP 服务整体不可用。

## 实现与学习顺序

1. **数据模型和命令基础**：围绕 String、Hash、List、Set、ZSet 建立业务选择表；手动练习 `SET`、`SETEX`、`GET`、`TTL`、`DEL`、`EXISTS`、`HSET`、`HGETALL`、`SADD`、`SMEMBERS`、`ZADD`、`ZRANGE`。重点理解 TTL 是 key 级属性。
2. **RedisClient 封装**：用 hiredis 写一个非线程安全、单连接客户端，核心手写 `get` 与 `setEx`。使用 `redisCommandArgv` 的长度参数传递 key/value，避免格式化命令在空格、二进制内容上的问题。
3. **单连接测试**：连接、写入、读取、TTL 过期、Redis 不可用降级。测试只在 Worker 侧调用同步 Redis，绝不在 EventLoop 回调中进行网络阻塞调用。
4. **连接所有权设计**：不让多个 Worker 共用一个 hiredis connection。先明确 Worker 独占连接或连接池的方案，再接入 L2，避免用一把全局 mutex 把 L2 再次做成串行瓶颈。
5. **L1 + L2 接入与压测**：分别测热、混合、冷查询及 Redis 故障场景，记录 QPS、P99、缓存命中率、Worker CPU 和 Redis 网络开销。结论必须以结果为准，不预设“加 Redis 一定更快”。

## 当前面试回答

“我这里选择 Redis String，而不是 Hash。搜索结果的缓存粒度是一个查询对应一份完整 JSON，读取模式是整体读写，`GET`/`SETEX` 平均 O(1)，且 TTL 可以直接绑定整个结果 key。Hash 更适合用户资料等一个实体内字段独立读写的场景；它的 field 不能独立设置 TTL。Redis 的 String 是二进制安全字节串，存 JSON 没问题。由于 Redis 是 L2 加速层，miss 或暂时不可用时会回源到本地搜索，而不会影响 EventLoop 的 I/O 线程。”
