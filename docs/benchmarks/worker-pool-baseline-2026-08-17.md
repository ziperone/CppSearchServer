# WorkerPool 压测基线（2026-08-17）

## 目标

验证 WorkerPool 的真实收益，而不是假定“多线程一定更快”。本轮重点回答两个问题：

1. 异步任务交接会给单请求增加多少固定成本？
2. 在多连接压力下，Worker 是否真正使用多核并提升吞吐与尾延迟？

## 可复用测试流程

所有性能数据必须使用 Release 构建、相同文档语料、相同 URL、相同压测时长和相同连接数比较。Debug 构建包含调试信息且通常不开优化，不能用于性能结论。

### 1. 构建 Release

在 Linux 项目目录执行：

```bash
cd /home/a704/Desktop/sseattack/CppSearchServer
cmake -S . -B build-ralease -DCMAKE_BUILD_TYPE=Release
cmake --build build-ralease
ctest --test-dir build-ralease --output-on-failure
```

含义：

- `cmake -S .`：源代码与 `CMakeLists.txt` 位于当前目录。
- `-B build-ralease`：将生成的构建规则、目标文件和可执行文件隔离到该目录。
- `CMAKE_BUILD_TYPE=Release`：选择优化构建配置。
- `cmake --build`：调用底层构建工具编译并链接。
- `ctest`：先确认正确性测试通过，再进行性能测试。

### 2. 启动服务

```bash
./build-ralease/cpp_search_server 18091 data/docs 4
```

参数依次是：端口、文档根目录、Worker 数量。此处 `4` 表示固定启动 4 个业务执行线程；EventLoop 仍为 1 个 I/O 调度线程。

另开一个终端确认接口可用：

```bash
curl --fail --silent --show-error \
  'http://127.0.0.1:18091/search?q=epoll'
```

成功时应返回包含 `query` 与 `results` 的 JSON。先做冒烟请求，避免把启动失败、端口冲突误判为性能问题。

### 3. 找到服务 PID

```bash
pgrep -af 'cpp_search_server 18091'
```

输出最左侧的数字是 PID。后续 `pidstat -p <PID>` 只监控这个服务进程，而非整台机器。

### 4. 测试单连接下限

```bash
wrk -t1 -c1 -d30s --latency \
  'http://127.0.0.1:18091/search?q=epoll'
```

- `-t1`：wrk 客户端使用 1 个压测线程，不代表服务器只有 1 个线程。
- `-c1`：仅维持 1 条 TCP Keep-Alive 连接。
- `-d30s`：持续 30 秒，减少瞬时波动。
- `--latency`：输出 P50/P75/P90/P99 延迟分位数。

这组数据不追求最高 QPS，而是观察每个请求在没有并行摊薄时的固定成本。

### 5. 测试常用并发平台点

```bash
wrk -t4 -c64 -d30s --latency \
  'http://127.0.0.1:18091/search?q=epoll'
```

`-c64` 表示 64 条同时保持的客户端 TCP 连接；每条连接独立按 HTTP 响应顺序请求。它们能持续给 WorkerPool 提供任务，适合观察多连接下的业务并行效果。

重点记录：

- `Requests/sec`：平均吞吐量（QPS），越高越好。
- `Latency 99%`：99% 请求不超过的延迟，反映尾部体验。
- `Avg`：平均延迟，不能单独替代 P99。
- `Transfer/sec`：网络传输量；响应大小变化时不能只用它比较性能。

### 6. 在压测期间监控 CPU、内存、I/O

先设定 PID：

```bash
PID=<上一步得到的服务 PID>
pidstat -p "$PID" -urd 1 31
```

含义：

- `-u`：CPU，`%usr` 是用户态计算，`%system` 是内核态网络/epoll/调度等。
- `-r`：内存，重点看 `RSS`（实际常驻物理内存）；`VSZ` 包含预留虚拟地址，线程栈会使它明显变大。
- `-d`：磁盘 I/O；查询阶段应接近 0，因为索引启动时已载入内存。
- `1 31`：每秒采样一次，共 31 次，覆盖 30 秒 wrk。

若希望一条命令完成压测与监控：

```bash
PID=<服务 PID>
pidstat -p "$PID" -urd 1 31 > /tmp/cpp-search-pidstat.log &
MONITOR_PID=$!
wrk -t4 -c64 -d30s --latency \
  'http://127.0.0.1:18091/search?q=epoll'
wait "$MONITOR_PID"
cat /tmp/cpp-search-pidstat.log
```

后台的 `pidstat` 与前台的 `wrk` 同时运行；`wait` 保证监控结束后再读取完整日志。

## 本轮结果

环境：Linux 本机回环请求，小型内存文档索引，Release，服务端 1 EventLoop + 4 Worker。

| 场景 | QPS | 平均延迟 | P99 |
|---|---:|---:|---:|
| `t1/c1` | 8,189.63 | 119.37us | 258us |
| `t4/c64` 第一次 | 131,673.59 | 485.77us | 634us |
| `t4/c64` 监控轮 | 120,385.97 | 529.35us | 693us |

监控轮 `pidstat` 平均值：

```text
%usr     179.81
%system   92.29
%CPU     272.10
RSS       3904 KB
disk I/O     0 KB/s
```

`272.10% CPU` 代表服务进程平均同时占用约 2.72 个逻辑核，说明 Worker 参与了多核计算；它不是超过整机 CPU 上限。RSS 很低且 I/O 为零，符合小型内存索引设计。

## 与旧 Keep-Alive 基线的初步比较

旧基线（同服务器、Release、`t4/c64`、Keep-Alive）约为 97.5k QPS、平均 651.74us、P99 1.27ms、70.42% CPU。

当前监控轮约为 120.4k QPS、平均 529.35us、P99 693us、272.10% CPU：

- 吞吐提高约 23.5%。
- P99 降低约 45%。
- CPU 消耗明显增加，但换来了更高吞吐和更低尾延迟。

单连接旧基线约 20.0k QPS，而当前为 8.2k QPS，说明任务队列、线程唤醒、eventfd 回传和状态检查确有固定成本。64 个连接足以持续填充 Worker，计算并行收益才超过该成本。

## 结论边界与下一组实验

当前数据支持“WorkerPool 在本项目的多连接场景有效”，但尚不能证明 4 个 Worker 是最佳配置，也不能排除压测波动。下一组必须控制变量：

```bash
# 对 worker_count 分别取 1、2、4、8，其他参数不变
./build-ralease/cpp_search_server 18091 data/docs <worker_count>
wrk -t4 -c64 -d30s --latency \
  'http://127.0.0.1:18091/search?q=epoll'

# 再以最佳 worker_count 测试连接饱和点
wrk -t4 -c256 -d30s --latency \
  'http://127.0.0.1:18091/search?q=epoll'
```

每个点至少运行两到三次，记录中位数。若 QPS 不再增长但 P99 上升，说明该点已接近饱和；继续增加 Worker 可能只增加调度和锁竞争。
