# Day 31: 缓存对照压测设计

## 为什么先增加运行时模式

比较缓存性能时，不能拿不同提交、不同构建参数或不同机器上的历史数字直接下结论。因此服务新增第六个启动参数：

```text
cpp_search_server <port> <documents_root> <workers> <io_loops> <cache_mode>
```

| 模式 | 行为 | 对照目的 |
|---|---|---|
| `none` | 完全跳过 L1 和 Redis | 真实检索基线 |
| `l1` | 仅当前 Worker 的本地 LRU | 观察去锁后的本地命中收益 |
| `l1-redis` | Worker-local L1 + Redis L2 | 观察共享缓存和网络开销 |

三种模式共享相同的 C++ 二进制、文档、Worker/I/O 数量和 HTTP 请求，仅改变缓存路径。

## 固定实验条件

```text
构建：build-ralease / Release
服务：4 Worker，4 I/O EventLoop
客户端：wrk -t4 -c256 -d30s --latency
查询：/search?q=epoll
```

每个模式至少运行 3 次。记录中位数或稳定区间，不根据单次最高 QPS 下结论。每次测试只停止自己通过 `$server_pid` 启动的服务，绝不使用 `pkill` 或影响其他端口上的用户进程。

## 热点查询流程

```bash
cd ~/Desktop/sseattack/CppSearchServer

port=18110
mode=none  # 依次替换为 l1、l1-redis

./build-ralease/cpp_search_server "$port" data/docs 4 4 "$mode" \
  >"/tmp/cpp-search-${mode}.log" 2>&1 &
server_pid=$!

until curl -fsS "http://127.0.0.1:${port}/search?q=epoll" >/dev/null; do sleep 0.1; done

# l1 和 l1-redis 需要预热；none 不需要。
if [ "$mode" != "none" ]; then
  curl -fsS "http://127.0.0.1:${port}/search?q=epoll" >/dev/null
fi

pidstat -u -r -d -p "$server_pid" 1 35 >"/tmp/${mode}-pidstat.txt" &
monitor_pid=$!

wrk -t4 -c256 -d30s --latency "http://127.0.0.1:${port}/search?q=epoll" \
  | tee "/tmp/${mode}-wrk.txt"

wait "$monitor_pid"
kill "$server_pid"
wait "$server_pid" 2>/dev/null || true
```

命令含义：

- `until curl ...`：确认服务已经实际监听，再开始压测；
- `pidstat ... 1 35`：以 1 秒间隔采集 35 次，覆盖 30 秒 wrk 与启动/结束余量；
- `$!`：记录刚启动的后台进程 PID；`wait` 确保监控结束后再读取文件；
- `kill "$server_pid"`：仅结束本次脚本启动的服务。

`l1-redis` 热点测试前可清理本项目的单条查询缓存，避免上次实验污染预热状态：

```bash
redis-cli DEL 'search-json:v1:top-k=10:q=epoll'
```

## 需要记录的指标

| 指标 | 解释 |
|---|---|
| QPS | 整体吞吐；必须和 P99 一起看 |
| Avg/P50/P99 latency | P99 反映排队、锁、网络或调度尾部延迟 |
| `%usr` / `%system` | 用户态检索/序列化与内核网络/epoll 成本的比例 |
| RSS | 常驻物理内存；L1 副本与内存泄漏风险 |
| Redis `INFO commandstats` | GET/SET 调用数，验证 L1 是否真正吸收热点 |

在 `l1-redis` 的纯热点稳态中，Redis 命令数不一定很高：每个 Worker 的 L1 预热后会直接返回。这不是 L2 无效，而是说明 L1 成为了更近的命中层。L2 的价值需要通过多个服务实例、Worker 冷启动或跨 Worker 首次命中实验进一步验证。

## 结果解释边界

- `none` 更快：当前倒排索引太轻，缓存路径成本大于节省的计算；
- `l1` 更快、`l1-redis` 变慢：本地热点有收益，但 Redis 网络/序列化对单机轻查询不划算；
- `l1-redis` 更快：必须同时确认命中率、Redis 延迟与 P99，不应只看 QPS；
- QPS 平台但 P99 上升：通常说明队列/连接/Worker 或 I/O Reactor 出现等待；
- Redis 不可用时必须仍能返回正确结果，性能会退回 `none` 或 L1 可覆盖的水平。

## 下一个实验

在热查询完成后，再增加有限查询集的混合负载，以及两实例共享 Redis 的冷启动验证。真正的“冷查询”需要查询 key 不重复但业务计算量可比；当前小语料中随机不存在词会改变检索成本，不能把它直接与 `epoll` 热点数据横向比较。
