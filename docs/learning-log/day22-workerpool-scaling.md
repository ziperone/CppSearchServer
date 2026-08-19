# Day 22: WorkerPool 扩展与瓶颈迁移

## 为什么学习这一部分

多 Reactor 已经将连接 I/O 分散到多个 EventLoop，但在 I/O=4、Worker=2 时两个 Worker 接近满核。这意味着不能凭感觉判断下一步，必须固定 I/O 数并单独改变 Worker 数。

## 实验设计

固定：Release、`/search?q=epoll`、`wrk -t4 -c256 -d30s --latency`、I/O Loop=4。

变量：Worker=1、2、4。

这是控制变量法：如果同时增加 I/O Loop 和 Worker，就无法判断吞吐变化究竟来自哪一层。

## 结果与理解

```text
Worker=1: 约 12.6 万 QPS，唯一 Worker 约 97%，I/O Loop 都较空闲
Worker=2: 约 26.7 万 QPS，两个 Worker 约 96%，I/O Loop 约 70%
Worker=4: 约 37.1 万 QPS，Worker 约 76%，I/O Loop 约 93%
```

Worker=1 到 2 的提升很大，因为原本只有一个 Worker 串行做检索；Worker=2 到 4 仍有有效提升，但提升不是翻倍，因为 I/O 和回传路径重新接近饱和。

## 核心结论

线程数不是越多越好，也不是“Worker 数等于 CPU 核数”就一定最佳。正确流程是：

```text
观察最先满载的线程
    -> 只扩容对应层级
    -> 压测
    -> 再观察新瓶颈
```

本项目已经完成两次有证据的迁移：

```text
单 I/O Loop 饱和
    -> 多 Reactor
    -> Worker 饱和
    -> WorkerPool 扩容
    -> I/O Loop 再次接近饱和
```

这比背诵“epoll + 线程池”更能体现后端工程能力：每个设计选择都有压测数据与线程级解释支持。

完整数据见 [多 Reactor I/O Loop 扩展压测](../benchmarks/multi-reactor-io-loop-scaling-2026-08-19.md)。
