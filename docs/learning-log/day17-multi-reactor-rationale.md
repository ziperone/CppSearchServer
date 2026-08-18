# Day 17: 多 Reactor 的设计依据与风险边界

## 当前成果

项目已具备：

```text
单 EventLoop + epoll/Channel/TcpConnection
HTTP Keep-Alive、半关闭、空闲超时与流水请求顺序
本地文档切块、倒排索引、TF-IDF TopK 检索
WorkerPool 异步检索 + eventfd 回传
Release 构建、单元测试、wrk/pidstat 压测记录
```

当前所有网络连接仍归属于同一个 EventLoop：它负责读取请求、提交 Worker、接收 eventfd 回传、写出响应、处理 Timer 和关闭连接。

## 数据依据

压测记录见 `docs/benchmarks/worker-pool-baseline-2026-08-17.md`。

- `c64` 下，Worker=1 约 141k QPS；更多 Worker 反而引入任务队列、eventfd 和调度成本。
- `c256` 下，Worker=2 比 Worker=1 有有限收益，说明高连接数时单 Worker 有部分积压；Worker=4/8 再次退化。
- `pidstat -t` 显示 c256、Worker=2 时：EventLoop 主线程约 96% CPU，其中约 72% 为内核态；两个 Worker 各约 59% CPU，主要是用户态计算。

这说明继续单纯增加 Worker 不能线性提升吞吐；单 EventLoop 的 I/O 与回传路径是下一阶段应验证的扩展点。

## 目标架构

```text
主 Reactor（acceptor）
-> accept clientfd
-> round-robin 选择子 Reactor
-> queueInLoop(clientfd 初始化任务)

子 Reactor（多个 I/O EventLoop）
-> 创建 Channel + TcpConnection
-> 将 clientfd 注册到自己的 epoll
-> 独占该连接的 Buffer、Channel、Timer、关闭和 send/recv
-> 将业务交给共享 WorkerPool

WorkerPool
-> 执行只读 SearchApplication 查询
-> 完成后经该 TcpConnection 所属子 Reactor 的 eventfd 回传
```

多 Reactor 的本质不是把“读线程”和“写线程”分开，也不会让同一条 TCP 连接并行读写。它是把**不同连接分片给不同 I/O Loop**，使不同连接的收发、回传和 Timer 能在多核上并行。

## 必须保持的归属规则

每条连接从创建到关闭只能归属一个子 EventLoop：

1. Channel 必须注册到所属子 Loop 的 epoll 实例。
2. TcpConnection 的 `loop_` 必须指向所属子 Loop；其 Timer 必须进入同一个 TimerQueue。
3. Worker 回调中的 `self->loop_.queueInLoop(...)` 必须回到所属子 Loop。
4. Buffer、Channel 状态、fd 关闭和 `request_generation_` 仍只由所属子 Loop 修改。

因此主 Reactor 只 accept，不能在自己线程中创建并注册 TcpConnection 后再交给子 Reactor。正确做法是把 `clientfd` 投递给目标子 Loop，并在目标线程创建 Channel/TcpConnection。

## 预期收益与代价

预期收益：

- 多组连接可并行 `recv/send`，降低单 EventLoop 串行收发压力。
- Worker 结果分散回各自 I/O Loop，降低单一 eventfd 回传队列压力。
- 每个 Loop 的 Channel、Timer 和连接状态天然线程隔离，无需为连接内部状态额外加锁。

可能恶化点：

- acceptor 到子 Loop 的一次 `queueInLoop + eventfd` 交接。
- 更多线程的调度、栈空间与缓存失效。
- 多个 I/O Loop 同时提交共享 WorkerPool，增加任务队列竞争。
- 简单 round-robin 只按连接数均分，长连接或慢连接可能造成负载不均。
- 关闭和析构顺序更复杂，必须保证 Worker 先停止、再销毁所属 EventLoop。

## 分阶段实施与验证标准

1. `EventLoopThread`：在线程内创建并运行一个 EventLoop，验证跨线程 `queueInLoop` 的任务在目标线程执行。
2. `EventLoopGroup`：维护多个 EventLoopThread，提供轮询 `nextLoop()`。
3. 改造 accept：只将非阻塞 clientfd 投递到目标 I/O Loop，在目标 Loop 内完成连接创建和 epoll 注册。
4. 增加连接归属与关闭场景测试。
5. 固定 Worker 数、查询语料和 wrk 参数，对比 1/2/4 I/O Loop 的 QPS、P99、线程 CPU、RSS 与错误率。

只有当多 Reactor 在相同负载下提高吞吐或降低 P99，且正确性测试全部通过，才保留该复杂度；否则保留单 Reactor + WorkerPool，并把多 Reactor 作为已验证但不适合当前短任务负载的设计结论。
