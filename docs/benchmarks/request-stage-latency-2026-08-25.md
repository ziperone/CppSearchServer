# 请求分阶段耗时诊断（2026-08-25）

## 目的

仅靠 QPS、P99 和 CPU 可以判断服务是否变快，却无法直接回答时间具体花在了哪里。本次增加可选的请求时间线，区分：

```text
完整 HTTP 请求已组装
-> 进入工作线程队列
-> 工作线程开始
-> 工作线程完成检索
-> 结果回到原网络线程
-> 首次 send
-> 全部响应字节交给内核发送缓冲
```

设计原则：指标默认关闭；开启后只用于诊断，不与无指标版本的 QPS 做绝对比较。

## 实现边界

- 每个请求携带独立时间线，不让工作线程直接修改 socket、收发缓冲或 epoll 状态。
- 工作线程记录排队开始、开始计算和结束计算；结果仍通过 eventfd 回到原网络线程。
- 网络线程记录结果回到 I/O、首次发送和全部发送完成，并以原子累计值汇总。
- `GET /metrics` 返回请求数、四段平均耗时、总平均耗时和最大总耗时。

计时从“已经得到一条完整 HTTP 请求”开始，到“响应字节全部写入内核发送缓冲”结束。因此它不包含客户端到服务端的传输时间、请求尚未完整前的等待时间，也不等同于 wrk 在客户端看到的完整延迟。

## 复现实验

```bash
cmake -S . -B build-ralease -DCMAKE_BUILD_TYPE=Release
cmake --build build-ralease -j

# 指标模式：最后一个参数 on
./build-ralease/cpp_search_server 18112 data/docs 4 4 none on
curl http://127.0.0.1:18112/metrics

# 或使用脚本；脚本启动和清理自己的服务进程
DURATION=10s bash scripts/run_latency_breakdown.sh
```

脚本固定使用 `wrk -t4 -c256 -d10s --latency` 请求热点查询 `/search?q=epoll`，并将 wrk 输出、JSON 指标和服务日志写到 `/tmp/cpp-search-latency-<timestamp>-*`。

## 本次结果

环境：远程 Linux、Release、4 个工作线程、4 个网络线程、256 条压测连接、无缓存、10 秒热点查询。

```text
wrk: 368,394.51 QPS
wrk 平均延迟: 686.98us
wrk P99: 0.99ms

服务端 /metrics:
completed_requests:     3,690,499
avg_worker_queue_us:          180
avg_worker_compute_us:          6
avg_result_return_us:         175
avg_response_write_us:          5
avg_total_us:                 368
max_total_us:               1,731
```

`completed_requests` 比 wrk 请求数略多，是脚本的就绪探测和 `/metrics` 请求也经过服务端的正常请求路径。服务端平均总耗时约 368us，小于 wrk 平均 687us，是因为两者起止点不同，且指标未涵盖客户端观察到的全部网络往返和请求接收等待。

## 当前判断

当前小型内存倒排索引下，工作线程实际检索平均只有约 6us；更大的时间主要在：

1. 等待工作线程开始执行，平均约 180us；
2. 工作结果通过队列和 eventfd 回到原网络线程，平均约 175us。

这不是“跨线程交接一定应该删除”的结论。它说明在当前轻量查询下，线程池和跨线程调度成本已经足以与业务计算竞争；若未来接入更重的 MySQL、RAG 或真实密文检索，计算时间变大后，需要重新测量收益边界。

## perf 采样状态

新增 `scripts/profile_cpu.sh`，默认以无指标服务启动进程。脚本先单独构建 `RelWithDebInfo + -fno-omit-frame-pointer` 的 `build-profile`，再执行：

```bash
DURATION=30 bash scripts/profile_cpu.sh
```

脚本会生成 `perf.data`、文本报告、wrk 输出和服务日志。当前远程服务器的 `/proc/sys/kernel/perf_event_paranoid` 为 `4`，普通用户无法执行 `perf record`，因此本次没有生成可信火焰图。脚本已在 perf 不能启动时提前失败并打印原因，避免出现“压测完成但没有采样报告”的假成功。

需要管理员明确授权后，才能通过降低 `perf_event_paranoid` 或授予合适的 `CAP_PERFMON`/相关权限采样；本项目没有尝试绕过该限制。

## 下一步

1. 在获得 perf 权限后，固定相同负载比较无缓存、L1、L1+Redis 的函数级热点。
2. 为阶段耗时增加分位数直方图和队列长度，而不是只看平均值。
3. 接入更重下游后，复测工作线程排队、检索、回传和发送各阶段的占比。
