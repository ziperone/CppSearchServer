# Day 18: EventLoopThread - I/O 线程的启动与安全退出

## 这一模块补上的缺口

单 Reactor 中，`EventLoop` 直接在主线程运行。多 Reactor 需要多个独立的 I/O 线程：每个子线程都要拥有自己的 `EventLoop`、`epoll`、`eventfd`、`TimerQueue` 和连接集合。

`EventLoopThread` 的职责只是管理其中**一个** I/O 线程的生命周期：

```text
主线程 startLoop()
    -> 创建 std::thread
    -> 等待子线程完成 EventLoop 初始化

子线程 threadMain()
    -> 在本线程栈上创建 EventLoop
    -> 发布 loop_ 指针并通知主线程
    -> loop.loop() 持续处理 I/O

析构
    -> 通知该 EventLoop 自行退出
    -> join，等待 I/O 线程彻底结束
```

它不负责连接分配，也不让其他线程直接修改 `Channel`、`Buffer` 或 `TcpConnection`。

## 为什么 EventLoop 必须在 threadMain 中创建

`EventLoop loop;` 是 `threadMain()` 的局部对象，因此它从创建到销毁都属于子 I/O 线程。后续连接注册到这个 Loop 后，连接的读写、定时器和关闭操作都在同一线程内完成，避免为连接内部状态增加大量锁。

`loop_` 只是把这个对象的地址安全地公布给启动者，供其调用 `queueInLoop()` 或 `quit()`；它不是所有者，也不能在子线程结束后继续使用。

## 启动同步：为什么不用 sleep

`startLoop()` 创建子线程后不能靠“睡一会儿”猜测子线程已经完成初始化。它使用：

- `mutex_`：保护跨线程共享的 `started_` 与 `loop_`；
- `state_changed_`：在子线程写入 `loop_` 后唤醒等待者；
- `wait(lock, predicate)`：若 `loop_ == nullptr`，原子地释放锁并休眠；醒来后重新加锁并再次检查条件。

这避免了两类竞态：主线程过早返回空指针，以及通知先发生、主线程后开始等待而永久睡眠。

## 退出同步：原子变量、wakeup 与 join 各自解决什么

`EventLoop::quit_` 是每个 EventLoop 实例自己的成员，不是全局变量。但控制线程会写它，I/O 线程会在 `loop()` 中读取它，因此这里存在跨线程读写，必须使用 `std::atomic<bool>`。

只修改 `quit_` 仍可能让 I/O 线程阻塞在 `epoll_wait()`；`quit()` 随后写入 `eventfd`，使 `epoll_wait()` 立即返回，再由循环读取到退出标记并结束。

析构函数的顺序：

1. 在 `mutex_` 保护下读取并调用 `loop_->quit()`，避免 `threadMain()` 同时将 `loop_` 置空并销毁其栈对象；
2. 释放锁；
3. `thread_.join()` 等待子线程退出，保证对象销毁后没有遗留线程。

`notify_one()` 的作用仅是唤醒正在等待 `loop_` 就绪的 `startLoop()`；它不关闭线程，也不替代 `join()`。当前没有任何线程等待“loop 已退出”，因此退出时不需要 `notify_all()`。

## 哪些地方需要锁，哪些不需要

- `started_`、`loop_`：由启动线程和 I/O 线程共同访问，使用 `mutex_`；
- `quit_`：控制线程写、I/O 线程读，使用原子变量；
- `Channel`、`TcpConnection`、输入输出 Buffer、TimerQueue：仍只允许所属 EventLoop 的线程访问，不为它们额外加锁；
- 跨线程投递业务结果：继续使用已有的 `queueInLoop()` 与 `eventfd`，不直接修改连接对象。

核心原则是：**先确定数据是否被多个线程同时访问；只有共享状态才需要同步，并优先保持连接状态单线程归属。**

## 验证

新增 `event_loop_thread_test`，验证：

1. `startLoop()` 返回后目标 EventLoop 已经可以接收任务；
2. `queueInLoop()` 的任务确实运行在子 I/O 线程，而非调用者线程；
3. 重复调用 `startLoop()` 会抛出 `std::logic_error`；
4. 离开作用域时析构函数能够安全 `quit + join`。

远程 Linux Release 构建执行 `ctest --test-dir build-ralease --output-on-failure`：**15/15 通过**。

## 面试表达

“我把一个 I/O Loop 封装为 EventLoopThread：子线程内创建 EventLoop，通过条件变量向启动线程发布就绪状态；退出时用原子 quit 标记配合 eventfd 唤醒 epoll_wait，并在析构中 join。这样下一步 EventLoopGroup 可以创建多个 I/O Loop，而单条连接仍然严格归属一个 I/O 线程，连接状态不需要跨线程加锁。”
