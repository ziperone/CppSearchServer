# Day 19: EventLoopGroup - 多个 I/O Loop 的组织与轮询

## 这一模块补上的缺口

`EventLoopThread` 只能运行一个子 I/O Loop。多 Reactor 还需要一个管理者，统一启动多个子线程，并让主 Acceptor 在每次成功 `accept(clientfd)` 后选择一个目标 Loop。

`EventLoopGroup` 不处理网络数据，不拥有连接，也不操作 WorkerPool。它只负责：

1. 创建固定数量的 `EventLoopThread`；
2. 调用 `startLoop()`，获得每个已经就绪的 `EventLoop`；
3. 使用 round-robin 返回下一个子 Loop。

```text
主 Acceptor: accept(clientfd)
        |
        v
EventLoopGroup::nextLoop()
        |
        v
目标子 EventLoop: queueInLoop(初始化 clientfd)
```

## 数据所有权

```cpp
std::vector<std::unique_ptr<EventLoopThread>> threads_;
std::vector<EventLoop*> loops_;
```

- `threads_` 是所有者。Group 独占每个 `EventLoopThread`，析构时会依次触发子线程的 `quit + join`；
- `loops_` 是非拥有指针缓存。`EventLoop` 在每个子线程的 `threadMain()` 栈上创建，Group 只保存 `startLoop()` 返回的地址用于快速选择；
- 因此 `loops_` 中的指针只在对应 `EventLoopThread` 运行期间有效，不能在 Group 析构后保存或使用。

## 启动与轮询

构造函数拒绝 `loop_count == 0`，并创建指定数量的 `EventLoopThread`。

`start()` 只能调用一次；它逐一启动子线程，将返回的 `EventLoop&` 转为地址存入 `loops_`。只有启动完成后 `started_` 才为真。

`nextLoop()` 的核心：

```text
返回 loops_[next_index_]
next_index_ += 1
若到达 loops_.size()，回到 0
```

当前 `nextLoop()` 只由一个主 Acceptor 线程调用，`next_index_` 没有跨线程共享，因此不需要锁。若未来多个线程都要分配连接，必须重新设计分配策略和同步方式。

## 连接归属原则

主线程**只选择目标 Loop 并投递 clientfd**。它不能先在自己的线程创建 `Channel/TcpConnection`、注册自己的 epoll，再把对象交给子 Loop。

原因是：`Channel` 注册的 epoll、`TcpConnection::loop_`、Buffer、Timer、关闭操作和 Worker 回传都必须属于同一个 I/O Loop。正确流程是在目标子线程的 `queueInLoop()` 回调中创建并注册连接。

## 验证

新增 `event_loop_group_test`，验证：

1. `EventLoopGroup(0)` 与启动前 `nextLoop()` 都会拒绝；
2. 两个子 Loop 连续选择四次的顺序为 `A -> B -> A -> B`；
3. 重复 `start()` 会拒绝；
4. 分别投递给 A、B 的任务运行在两个不同的 I/O 线程，而不是调用者线程；
5. Group 离开作用域后，内部的 `EventLoopThread` 能安全退出并 join。

远程 Linux Release 构建执行 `ctest --test-dir build-ralease --output-on-failure`：**16/16 通过**。

## 面试表达

“为了从单 Reactor 扩展到多 Reactor，我用 EventLoopGroup 管理固定数量的 EventLoopThread。主 Acceptor 按 round-robin 为新 clientfd 选择子 I/O Loop，并通过 queueInLoop 投递初始化任务。每条连接从 epoll 注册、收发、定时器到关闭都固定归属一个子 Loop，避免连接内部状态跨线程共享。”
