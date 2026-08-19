# Day 20: 多 Reactor 接入 - 主 Acceptor 向子 I/O Loop 转交连接

## 这一模块完成了什么

服务器从“一个 EventLoop 接受并管理所有连接”变为：

```text
主 EventLoop（Acceptor）
    -> accept(clientfd)
    -> 设置 clientfd 为非阻塞
    -> EventLoopGroup::nextLoop() 轮询选择子 I/O Loop
    -> target_loop.queueInLoop(连接初始化任务)

目标子 EventLoop
    -> 创建 Channel
    -> 创建 TcpConnection
    -> 在自己的 epoll 注册 clientfd
    -> 管理这条连接的读、写、Buffer、Timer、关闭和 Worker 回传
```

多 Reactor 的本质是按连接分片，而不是把读线程和写线程拆开。每条 TCP 连接仍然只归属一个子 I/O Loop，因此连接内部仍保持单线程访问。

## 为什么 clientfd 能由主线程先处理

刚刚 `accept()` 返回的 `client_fd` 只是新连接的内核描述符，尚未绑定到项目中的 `Channel`、`TcpConnection`、epoll、Buffer 或 Timer。主 Acceptor 可以在投递前调用 `setNonBlocking(client_fd)`。

但从 `Channel/TcpConnection` 创建开始，连接必须只在目标子 Loop 中初始化：

- `Channel` 要向目标 Loop 的 epoll 注册；
- `TcpConnection::loop_` 必须指向目标 Loop；
- 连接的 Buffer、Timer、关闭逻辑和 Worker 完成回调都只能回到这一条 Loop；
- 若主线程创建后再转交，会形成同一连接跨 I/O 线程的生命周期，导致状态竞争和错误的 epoll/Timer 归属。

## 连接投递任务

```cpp
net::EventLoop* target_loop = &io_loops.nextLoop();
target_loop->queueInLoop([target_loop, client_fd, request_handler] {
    auto client_channel = std::make_shared<net::Channel>(client_fd);
    auto connection = std::make_shared<net::TcpConnection>(
        *target_loop, client_fd, request_handler);
    connection->establish(client_channel);
});
```

- `target_loop`、`client_fd`、`request_handler` 都按值捕获；
- `client_fd` 不会在下一轮 accept 循环被覆盖；
- 任务通过目标 Loop 自己的 `eventfd` 唤醒并在目标 I/O 线程执行；
- `EventLoopGroup` 在 `serveWithEventLoop` 内持续存活，因此 `target_loop` 在服务器运行期间有效。

## 命令行参数链路

当前命令格式：

```bash
./cpp_search_server <port> <docs_root> <worker_count> <io_loop_count>
```

`argv[0]` 是可执行文件自身，因此：

| 字段 | argv 下标 | 最小 argc |
| --- | --- | --- |
| port | `argv[1]` | 2 |
| docs_root | `argv[2]` | 3 |
| worker_count | `argv[3]` | 4 |
| io_loop_count | `argv[4]` | 5 |

未传第 4 个用户参数时，`parseIoLoopCount()` 返回默认值 `1`，保留原有三参数命令的兼容性。返回的整数按值传给 `serveWithEventLoop`，后者用它创建 `EventLoopGroup`。

## 生命周期顺序

`serveWithEventLoop` 中声明顺序：

```text
accept_loop -> io_loops -> workers
```

C++ 离开作用域时按反序析构：

```text
workers 先 stop + join
    -> io_loops 的每个 EventLoopThread 再 quit + join
        -> accept_loop 最后销毁
```

因此 Worker 不会在子 I/O Loop 已销毁后继续通过 `queueInLoop` 回传结果。

## 验证

1. 远程 Linux Release 构建与 `ctest --test-dir build-ralease --output-on-failure`：**16/16 通过**；
2. 旧命令兼容：`./build-ralease/cpp_search_server 18093 data/docs 2`，`/search?q=epoll` 返回 HTTP 200 和 336 字节 JSON；
3. 双 I/O Loop：`./build-ralease/cpp_search_server 18094 data/docs 2 2`，并发请求 `epoll`、`reactor` 分别成功返回 336、338 字节 JSON；
4. `event_loop_group_test` 单独验证两个子 Loop 的轮询选择为 `A -> B -> A -> B`，且任务在两个不同 I/O 线程执行。

上述验证证明功能可用和连接选择逻辑正确；是否带来性能收益仍需在固定参数下使用 `wrk + pidstat -t` 比较 `io_loop_count=1/2/4` 的 QPS、P99、线程 CPU 与 RSS，不能仅凭功能测试下结论。

## 面试表达

“我将单 Reactor 改为主从多 Reactor：主 Loop 只 accept 并以 round-robin 选择子 I/O Loop，通过 eventfd 驱动的 queueInLoop 在目标线程创建 Channel 和 TcpConnection。每条连接从 epoll 注册、Buffer、Timer 到 Worker 结果回传都固定归属同一 I/O Loop，并保留单 I/O Loop 参数以做基准对比。”
