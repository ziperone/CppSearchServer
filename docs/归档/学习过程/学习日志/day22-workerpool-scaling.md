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

## 面试追问：如果还要继续提升 QPS，怎么做

**回答主线：先用当前数据定位，再小步扩容验证，不盲目堆线程。**

当前 `I/O=4 + Worker=4` 的线程级 CPU 显示：4 个 I/O Loop 各约 93%，4 个 Worker 各约 76%。因此下一步优先假设是 I/O 层先接近上限，而不是 Worker 计算不足。

1. 固定 Worker=4、请求、连接数和 Release 构建，只测试 I/O Loop=6、8；
2. 每个配置至少重复一次，记录 QPS、P50/P99、错误率、每线程 user/system CPU、RSS、上下文切换和任务队列等待时间；
3. 若 QPS 有明显增长且 P99 没有恶化，说明 I/O 分片仍有收益；若 QPS 平台化、P99 上升或上下文切换显著增加，就停止继续加 I/O 线程；
4. 若 I/O 不再是瓶颈而 Worker 又接近满载，再单独扫描 Worker=6、8；同时检查当前共享 WorkerPool mutex 队列是否出现竞争，必要时才评估分片队列、work-stealing 或批量提交；
5. 如果单机扩容收益递减，转而减少每个请求的工作量：L1 LRU、Redis L2、请求合并、缓存空结果和更高效的数据结构；
6. 单机达到可接受效率但容量仍不足时，使用负载均衡部署多个无状态 SearchServer 实例，以 Redis 共享热点缓存、MySQL 管理元数据，实现水平扩容。

不能跳过第 2 步直接声称“多加线程就会更快”。增加线程可能引入上下文切换、共享队列锁竞争、Cache miss、网络/内核队列压力，最终表现为 QPS 不再提升而 P99 变差。
