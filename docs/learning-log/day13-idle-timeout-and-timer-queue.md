# Day 13：Keep-Alive 空闲超时与 TimerQueue

## 本日目标

为 Keep-Alive 连接实现安全的空闲超时回收，避免长期无请求的连接持续占用 fd 和 Channel；同时不因高 QPS 请求而在定时器堆中积累大量无效任务。

## 模块边界

| 模块 | 职责 |
|---|---|
| `TimerQueue` | 维护到期时间与回调，不理解连接、fd、HTTP。 |
| `EventLoop` | 用最近 Timer 的剩余时间作为 `epoll_wait(timeout)` 参数；分发 fd 事件后执行到期回调。 |
| `TcpConnection` | 记录连接活动、判断是否真正空闲，并负责关闭 fd 与移除 Channel。 |

## TimerQueue

内部使用最小堆。堆顶总是最早到期 Timer：

- 查看最近到期时间：`O(1)`。
- 插入和弹出 Timer：`O(log N)`。
- 堆为空时返回 `-1`，允许 `epoll_wait(-1)` 无限等待。
- 堆顶已到期时返回 `0`，使 `epoll_wait(0)` 立即返回并处理 Timer。

`runExpiredTimers()` 不返回 Timer 给 EventLoop，而是在内部逐个 `pop()` 并执行回调。回调结束后局部 Timer 与其中的 `std::function` 自动销毁。

## EventLoop 时间与 I/O 协作

```text
now = Clock::now()
timeout = TimerQueue::millisecondsUntilNextTimer(now)
epoll_wait(timeout)
-> 先处理本轮全部 fd 事件
-> 再运行已到期 Timer 回调
```

若 fd 在超时前就绪，`epoll_wait` 提前返回；若没有 fd 事件，到 timeout 后返回 0。先处理 fd 的顺序允许临界时刻收到的数据先刷新连接活动版本，再使旧 Timer 失效。

## 从“每次活动新建 Timer”到“单连接单 Timer”

第一版方案会在每次收发数据时注册新 Timer，再由旧 Timer 到期时检查 generation。这在高 QPS 下会在堆中积累大量尚未到期的无效 Timer，不适合当前约 9.7 万 QPS 的服务基线。

最终方案：每个连接同一时刻只保留一个 Timer。

```text
建立连接
-> markActivity(): 更新 last_activity_at_，generation++
-> 注册首个 Timer

后续成功 recv/send
-> markActivity()
-> 只更新时间和 generation，不注册新 Timer

唯一 Timer 到期后已被 pop
-> generation 不匹配：按 last_activity_at_ + timeout 注册下一轮唯一 Timer
-> generation 匹配且连接真正空闲：close()
-> generation 匹配但仍有未完成输入/输出：延后检查
```

## 安全关闭

Timer 回调捕获 `weak_ptr<TcpConnection>` 和 `expected_generation`：

```text
weak_ptr.lock() 失败
-> 连接已经析构，直接返回

generation 不匹配
-> 本次计时期间发生过活动，不关闭

generation 匹配 + isIdle()
-> TcpConnection::close()
-> EventLoop::removeChannel(fd)
-> epoll_ctl DEL
-> close(fd)
```

`weak_ptr` 不延长连接生命周期；Timer 到期时不会访问已经销毁的对象。

## 验证

- `timer_queue_test`：最早到期、未到期、同一轮多个到期任务。
- `event_loop_timer_test`：无 socket 时，Timer 能通过 `epoll_wait(timeout)` 唤醒 EventLoop 并退出。
- `tcp_connection_idle_timeout_test`：20ms 无活动连接关闭，对端收到 EOF。
- `tcp_connection_idle_refresh_test`：20ms 收到请求后，连接越过原 40ms 超时点仍可用，随后在刷新后的期限关闭。
- Release 环境总计 11 个测试全部通过。

## 线程池前置风险

当前所有 fd、Channel、Buffer、TimerQueue 和连接状态都由单 EventLoop 线程修改，无需锁。引入 Worker 后必须保持这一所有权纪律：

```text
Worker：检索、排序、JSON 构造等纯计算
EventLoop：fd、Channel、Buffer、连接关闭、generation、TimerQueue
```

Worker 完成任务后应通过 `eventfd` 唤醒所属 EventLoop，再由 EventLoop 检查连接和任务版本有效性并写入输出缓冲区。连接存在 Worker 任务时不属于空闲连接；它应由独立的请求处理超时管理，而不是 idle timeout。

## 后续边界

- 仅实现空闲读连接超时；输出缓冲区长期不可写属于写超时问题。
- 未实现跨线程 `runAfter()`；Worker 不能直接操作 TimerQueue。
- `Connection: close`、对端半关闭等主动终止路径仍优先遵循原有关闭逻辑。
