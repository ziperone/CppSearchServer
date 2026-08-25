# Day 21: 多 Reactor 的压测闭环

## 学习目标

完成一次完整的“提出瓶颈假设 -> 架构改造 -> 控制变量压测 -> 线程级解释 -> 决定下一步”的后端性能闭环。

## 本次假设

单 I/O EventLoop 在高连接数下接近一个 CPU 核；即使 Worker 已能并行检索，所有连接的 `recv/send`、eventfd 结果回传、Timer 与关闭仍集中到一个 Loop，可能形成排队。

## 如何验证

只改变 `io_loop_count`，固定 Release 构建、查询、Worker=2、`wrk -t4 -c256 -d30s --latency` 和文档目录。使用 `pidstat -t` 看线程而不是只看进程总 CPU。

## 得到的结果

I/O Loop 从 1 增至 2、4 时，QPS 从约 13.4 万增至 22.4 万、26.7 万，P99 从 2.36ms 降至 1.59ms、1.30ms。结果证明这次多 Reactor 增加的复杂度有实际收益。

更关键的是看到了瓶颈迁移：

```text
改造前：单个子 I/O Loop 接近 100% CPU
改造后：多个 I/O Loop 分摊收发；两个 Worker 接近 100% CPU
```

性能优化不是“让所有线程都加倍”，而是找到当前最先饱和的资源，再用数据判断是否值得改动。

## 必须记住的边界

- QPS 上升说明同一时间完成的请求更多，不代表每一段代码都更快；
- P99 降低说明高尾部排队减少；
- I/O=4 的 I/O 线程未满核、Worker 满核，说明下一限制更可能在业务计算或共享 WorkerPool；
- 不能由一次功能测试推断性能，也不能由一次压测推断长期稳定性；本次对 I/O=4 做了复测，QPS 波动约 1.4%。

完整数据见 [多 Reactor I/O Loop 扩展压测](../benchmarks/multi-reactor-io-loop-scaling-2026-08-19.md)。
