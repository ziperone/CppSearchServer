# Day 14：eventfd 与跨线程任务回传

## 本日目标

建立从其他线程安全交还任务给所属 EventLoop 的通道。WorkerPool 尚未接入；本阶段只验证“任务入队后，即使 EventLoop 正在 `epoll_wait(-1)`，也能被立即唤醒并在 EventLoop 线程执行”。

## 为什么需要

未来 Worker 不能直接操作 fd、Channel、Buffer 或 TimerQueue。它只能完成检索、排序和 JSON 构造，再把“写入响应”的任务交还给 EventLoop。

普通内存队列无法唤醒正在 `epoll_wait(-1)` 的线程，因此必须使用一个可被 epoll 监控的内核事件。

## eventfd 的角色

`eventfd` 是内核维护 64 位计数器的文件描述符：

```text
write(eventfd, 1)
-> 计数器增加
-> eventfd 变为 EPOLLIN 可读
-> epoll_wait 返回

read(eventfd)
-> 读出并清空计数器
-> eventfd 恢复不可读
```

它不是 socket，也不承载响应内容。当前模块只关注 `EPOLLIN`；`EPOLLOUT` 对 eventfd 几乎总是就绪，注册它会造成无意义唤醒。

## 设计

`EventLoop` 创建并管理 `wakeup_fd` 和对应 Channel：

```text
其他线程
-> queueInLoop(task)
-> mutex 保护下 pending_tasks_.push_back(task)
-> write(wakeup_fd, 1)

EventLoop
-> epoll_wait 返回 wakeup_fd 的 EPOLLIN
-> handleWakeup()
-> read(wakeup_fd)
-> mutex 保护下将 pending_tasks_ swap 到局部 tasks
-> 解锁
-> 逐个执行 tasks
```

入队必须在写 eventfd 之前。若先写 eventfd，EventLoop 可能醒来、看到空队列后重新阻塞，随后任务才入队且没有新的唤醒信号，导致任务卡住。

## 为什么使用 swap

EventLoop 不能持有 `pending_tasks_mutex_` 执行任务：任务可能慢、可能继续投递任务，持锁执行会阻塞所有 Worker 入队。正确做法是快速交换共享队列到本地 vector，立即解锁，再在 EventLoop 线程执行本地任务。

## 并发时序保证

若 Worker 在 EventLoop `read(eventfd)` 之后才入队：

- 若 Worker 先获得队列锁，当前 `swap` 会带走该任务；其 `write(eventfd)` 留下的额外可读通知将在下一轮被清空，队列为空也安全。
- 若 EventLoop 先 `swap`，Worker 随后入队并写 eventfd；下一轮 `EPOLLIN` 会取走新任务。

两种情况都不会丢任务。eventfd 可以出现多余唤醒，但不能遗漏任务。

## 验证

`event_loop_task_queue_test`：

1. 主线程创建并运行 EventLoop，初始没有业务 fd 或 Timer，进入 `epoll_wait(-1)`。
2. 一个真实 `std::thread` 等待后调用 `queueInLoop()`。
3. 回传任务断言自身运行在线程创建 EventLoop 的主线程，并调用 `loop.quit()`。

Release 环境总计 12 个测试全部通过。

## 下一步

实现最小 `WorkerPool`：Worker 只执行纯计算任务，完成后通过 `queueInLoop()` 回传。接入请求前还需要为连接增加 `processing_request` 和任务版本，避免 idle timeout 或过期结果误处理。
