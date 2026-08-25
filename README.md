# CppSearchServer

面向个人求职知识库的 C++ 高并发本地文档检索服务。它将 Markdown/TXT 文档按标题切分为可检索 chunk，建立倒排索引，并通过 HTTP 接口提供低延迟关键词搜索。

项目不是单纯复现 WebServer，而是围绕一个真实问题展开：简历、项目记录、面试经验和理论笔记分散在本地文件中，手动查找效率低。服务提供：

```text
GET /search?q=epoll%20reactor
```

返回命中的文档片段、标题路径、来源和排序分数；后续可作为 Agent/RAG 的关键词检索后端。

## 架构

```text
HTTP client
  -> Acceptor EventLoop
  -> child I/O EventLoop (epoll + Channel + TcpConnection)
  -> WorkerPool
  -> SearchApplication
  -> Worker-local L1 / Redis L2 / inverted index
  -> eventfd 回到原 I/O EventLoop
  -> HTTP response
```

- 非阻塞 socket、epoll Reactor、Channel/EventLoop/TcpConnection 分层；
- 输入输出缓冲区、EPOLLIN/EPOLLOUT、Keep-Alive、半关闭和空闲定时器；
- eventfd 将 Worker 结果安全交回连接所属 I/O 线程，Worker 不直接操作 fd、Channel 或 output buffer；
- 主 Acceptor + 多子 Reactor，WorkerPool 执行业务检索；
- Markdown 标题分块、Tokenizer、倒排索引、多关键词召回与 TopK 排序；
- Redis String 缓存完整搜索 JSON，Worker-local L1 和 Redis L2 均可运行时切换；
- CTest 覆盖网络、HTTP、检索、定时器、异步回调、Redis TTL 与故障降级。

## 缓存结论

缓存实现并不等于缓存一定更快。项目在同一 Release 二进制、4 Worker、4 I/O EventLoop、`wrk -t4 -c256 -d30s` 下，对 `/search?q=epoll` 各运行三次：

| 模式 | 平均 QPS | 平均 P99 |
|---|---:|---:|
| `none` | 375,418 | 0.94 ms |
| `l1` | 296,273 | 1.29 ms |
| `l1-redis` | 283,297 | 1.32 ms |

当前小型内存倒排索引下，缓存热路径成本超过节省的检索计算，故默认模式为 `none`。Redis L2 保留用于跨实例共享和未来 MySQL/RAG 等昂贵下游；双实例实验已验证实例 B 可以只读 Redis 而不回源写入。

缓存实现、三轮对照数据和默认关闭原因见[检索与缓存取舍](docs/02_核心实现/检索与缓存取舍.md)。

## 构建与运行

Linux 依赖：CMake、C++17 编译器、hiredis。Redis 缓存模式及 Redis 集成测试需要运行中的 Redis Server。

```bash
cmake -S . -B build-ralease -DCMAKE_BUILD_TYPE=Release
cmake --build build-ralease -j
ctest --test-dir build-ralease --output-on-failure
```

启动服务：

```bash
# <port> <documents_root> <workers> <io_loops> <cache_mode> <metrics_mode>
./build-ralease/cpp_search_server 18085 data/docs 4 4 none off

curl "http://127.0.0.1:18085/search?q=epoll%20reactor"
```

缓存模式：

```text
none      不使用缓存（默认，当前热点压测最快）
l1        仅 Worker-local L1
l1-redis  Worker-local L1 + Redis L2
```

请求分阶段耗时指标默认关闭。诊断时将最后一个参数设为 `on`，再请求 `/metrics`：

```bash
./build-ralease/cpp_search_server 18085 data/docs 4 4 none on
curl http://127.0.0.1:18085/metrics
```

## 验证与文档

```bash
# 三模式热点对照压测
bash scripts/run_cache_benchmark.sh

# 请求分阶段耗时诊断（指标模式，不用于与基准 QPS 直接比较）
bash scripts/run_latency_breakdown.sh

# perf CPU 采样（单独构建 RelWithDebInfo；需要系统允许 perf_event）
bash scripts/profile_cpu.sh

# 两实例共享 Redis L2 验证
bash scripts/verify_redis_l2_cross_instance.sh
```

- [文档导航](docs/00_导航.md)
- [项目定位与架构](docs/01_项目总览/项目定位与架构.md)
- [网络与并发实现](docs/02_核心实现/网络与并发实现.md)
- [检索与缓存取舍](docs/02_核心实现/检索与缓存取舍.md)
- [性能证据与复现](docs/03_性能与验证/性能证据与复现.md)
- [公开项目讲解提纲](docs/04_面试材料/公开项目讲解提纲.md)

## 下一阶段

Redis 阶段已结束。下一阶段引入 MySQL 作为文档元数据、分类与版本的权威数据源，Redis 继续承担版本化查询结果缓存；在此基础上再讨论多实例部署和分布式一致性问题。
