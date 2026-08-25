# Day 15: WorkerPool 与多连接业务并行

## 本日目标

在单线程 EventLoop 之外建立最小可用的 `WorkerPool`。它只执行不触碰连接状态的业务任务，为后续将文档检索移出 I/O 线程做准备。

## 先区分两种能力

```text
I/O 并发：一个 EventLoop 通过 epoll 轮转大量就绪 fd，不会等待某个连接的业务计算。
业务并行：多个 Worker 线程同时计算不同连接的检索任务，使用多个 CPU 核。
```

一个 EventLoop 并不会为每个 fd 创建线程。它仍在单线程中顺序执行 Channel 回调；真正的并行发生在多个 Worker 同时取到不同任务后。

同一条 TCP 连接保持串行请求-响应顺序。原因不是端口只能发送一份数据，而是同一连接上的 HTTP 字节流必须按顺序返回。不同连接可以独立完成、独立发送。

## 连接 A 与连接 B 的完整时序

```text
1. fd=A、fd=B 分别出现 EPOLLIN，epoll_wait 返回一批就绪事件。
2. EventLoop 根据 fd 找到 Channel，依次调用两个 TcpConnection 的读回调。
3. TcpConnection 读取并解析请求，将纯计算任务交给 WorkerPool。
4. Worker-1 计算 A，Worker-2 同时计算 B；EventLoop 已返回处理其他 fd 或阻塞等待。
5. Worker 完成后不能直接 send 或改 Buffer，而是 queueInLoop(结果任务) 并 write(eventfd, 1)。
6. EventLoop 被 eventfd 唤醒，在自己的线程中把结果写入对应连接的 output_buffer，再执行 send。
```

若 A 在前一个响应未发送完成时又收到 A2：A2 可以留在 `input_buffer`，但不提交 Worker；等 A1 响应完整发送后，才按顺序提交 A2。B 的任务不受影响。

## WorkerPool 的设计

```text
submit(task)
-> mutex 保护 tasks_.push(task)
-> 解锁
-> condition_variable.notify_one()

workerLoop()
-> wait(stopping_ || !tasks_.empty())
-> 持锁取走一个 task
-> 解锁
-> 执行 task
```

- 取走任务后必须先解锁再执行，否则一个慢任务会长期占住任务队列，其他 Worker 无法取任务。
- 每次提交一个任务使用 `notify_one()`；析构时使用 `notify_all()`，唤醒所有 Worker。
- 停止条件是 `stopping_ && tasks_.empty()`，因此析构会先排空已提交任务，再让线程退出并 `join()`。
- 当前单请求提交场景不需要批量接口。将来离线批处理可加入 `submitBatch`，减少加锁和唤醒次数；在线请求不能为了凑批次牺牲延迟。

## 性能判断

WorkerPool 不天然提高性能：任务很短时，加锁、条件变量、线程调度和 eventfd 回传可能比检索本身更贵；任务较重时，多个 CPU 核可并行计算，收益才会显现。接入后必须通过压测判断真实结果。

## 验证

`worker_pool_test` 使用两个会等待释放信号的任务：测试必须观察到两个任务都已启动，才能证明两个 Worker 可同时执行。Release 环境共 13 个测试全部通过。

## 下一步

把 `TcpConnection` 从同步 `RequestHandler -> Response` 改为异步回调接口，并新增 `processing_request_` 状态：Worker 只计算，结果经 eventfd 回到 EventLoop 后才写入连接缓冲区。还要把该状态纳入空闲连接判断，避免计算中的连接被 idle timer 误关。
