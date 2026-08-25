# 恢复学习：网络主线与 EventLoop

## 本次回顾范围

- 阻塞式 HTTP Server：`socket -> bind -> listen -> accept -> recv -> send`。
- `epoll` 的就绪事件通知。
- `Epoller` 与 `EventLoop` 的职责边界。

## 已掌握

- 单线程 epoll 是并发，不等于多核并行。
- `client_fd` 是连接句柄；HTTP 字节由 `recv` 从内核 socket 接收缓冲区读到用户态。
- `epoll` 解决等待 I/O 时被单个慢连接阻塞的问题，但不解决耗时业务的 CPU 并行。

## 本次纠正

1. `epoll_wait` 返回本轮的就绪事件及其 fd，不返回 HTTP 数据，也不是返回全部已注册 fd。
2. `listen_fd` 可读表示 accept 队列中存在已完成 TCP 握手的连接；`client_fd` 可读通常表示其接收缓冲区中有字节可读。
3. `Epoller` 负责注册和等待就绪事件；`EventLoop` 保存 fd 到回调的映射，并分发事件。
4. 当前 `EventLoop` 可以阻塞等待事件，但它在同一线程中依次执行就绪回调，因此是并发而非并行。

## 费曼结论

`epoll_fd` 像内核的事件登记簿：登记想关注的 fd，等待时只报告本轮有事的 fd。`EventLoop` 根据报告找到对应回调并执行；数据本身仍要通过 `recv` 读取。

## `serveWithEventLoop` 的关键顺序

1. `addReadEvent(listen_fd, accept_callback)` 同时保存 `listen_fd -> callback` 映射，并通过 `epoll_ctl(ADD)` 登记对 `EPOLLIN` 的关注。
2. `EventLoop::loop()` 调用 `epoll_wait` 阻塞等待；只有内核报告 `listen_fd` 可读后，才会执行 `accept_callback`。
3. `listen_fd` 可读只表示 accept 队列里至少有一个已完成 TCP 握手的连接。回调里的 `while` 不负责判断就绪，而是连续 `accept` 取走当前队列中的所有连接，直到非阻塞 `accept` 返回 `EAGAIN/EWOULDBLOCK`。
4. 每个新得到的 `client_fd` 再通过 `addReadEvent(client_fd, client_callback)` 注册给 epoll。以后该客户端发送字节时，epoll 才会报告这个 `client_fd` 可读。

## 当前最小版本的边界

- `EventLoop::quit()` 没有调用点；`Ctrl+C` 不是优雅退出。
- 仅修改 `quit_` 不能唤醒阻塞中的 `epoll_wait(-1)`；后续可用 `eventfd` 或超时机制实现优雅退出。
- `EPOLLHUP` 路径目前取消注册但未关闭 fd，属于后续 `Channel/Connection` 阶段需要补齐的生命周期问题。

## 最终费曼总结（修正版）

服务器先用 `socket` 创建监听 socket，再用 `bind` 绑定端口、用 `listen` 开始监听，得到 `listen_fd`。`listen_fd` 被注册到 epoll 后，内核在 accept 队列中存在已完成 TCP 握手的连接时，报告它可读。EventLoop 调用其 accept 回调，在循环中不断 `accept`，为每个新连接在服务器进程中返回一个 `client_fd`，直到返回 `EAGAIN` 表示当前队列已取空。每个 `client_fd` 随后被注册到 epoll，表示以后关注它是否有数据可读。

客户端发送 HTTP 请求后，内核报告对应 `client_fd` 可读；`epoll_wait` 返回本轮就绪事件和 fd，EventLoop 在自己的回调表中找到该 fd 的回调，再由 `recv` 从内核接收缓冲区读取字节，解析请求、执行业务、通过 `send` 写回响应。阻塞式版本会在单个连接的 `recv`、业务处理或 `send` 中等待，后续连接无法及时处理。epoll 只解决 I/O 就绪通知和等待问题，单线程 EventLoop 的回调仍按顺序执行，所以是并发而不是并行；并行需要线程池或其他多线程执行机制。

注意：epoll 内核只维护 fd 的关注事件并返回就绪事件，不保存或执行 C++ 回调；`fd -> callback` 映射属于 EventLoop 的用户态数据结构。
