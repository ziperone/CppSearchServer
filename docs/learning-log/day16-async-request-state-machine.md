# Day 16: 异步请求状态机与 WorkerPool 接入

## 本日目标

将文档检索从 EventLoop 线程移入 WorkerPool，同时保持连接 I/O、缓冲区、fd 生命周期仍只由 EventLoop 管理。

## 同步版本的瓶颈

旧版 `processRequest()` 直接调用 `SearchApplication::handleRequest()`：

```text
EPOLLIN
-> TcpConnection::handleRead
-> processRequest
-> 检索、排序、生成 JSON
-> send
```

检索期间 EventLoop 无法处理其他连接。即使 epoll 能一次返回多个就绪 fd，也只有 I/O 并发，没有业务并行。

## 异步接口

```cpp
using ResponseCallback = std::function<void(ResponseResult)>;
using RequestHandler =
    std::function<void(std::string request, ResponseCallback complete)>;
```

请求必须按值使用 `std::string`。`input_buffer_` 在提取请求后会 `retrieve()` 消费字节；若 Worker 持有 `string_view`，将可能访问已失效的缓冲区内存。

## 状态机

新增：

```cpp
bool processing_request_;
std::uint64_t request_generation_;
```

```text
完整请求到达
-> processRequest 提取独立 request
-> processing_request_ = true
-> request_id = ++request_generation_
-> 提交 WorkerPool

Worker 完成
-> complete(result)
-> EventLoop::queueInLoop
-> write(eventfd, 1)
-> EventLoop 被唤醒
-> finishRequest(request_id, result)
-> processing_request_ = false
-> output_buffer_ 写入结果
-> handleWrite 发送
```

`finishRequest()` 必须运行在 EventLoop 线程。它检查：连接 fd 是否有效、连接是否仍在处理请求、`request_id` 是否匹配。任一不满足则丢弃迟到或重复结果。

## 同连接顺序与多连接并行

同一连接在 `processing_request_ = true` 时仍可继续 `recv` 并累积下一条请求，但不能提交下一项业务任务。只有当前响应完整发送后，才从 `input_buffer_` 取下一条请求。

不同连接没有这个互相等待关系：连接 A 和 B 的任务可分别由不同 Worker 同时计算。结果谁先完成，谁先通过 eventfd 回到 EventLoop；每条连接内部仍保持自己的顺序。

## 生命周期与关闭

- 客户端 `shutdown(SHUT_WR)` 后，`recv` 返回 0，代表对端不再发送请求，但仍可接收服务端响应。
- 若 `processing_request_ = true`，不能因 `peer_closed_` 立即关闭；应等 Worker 结果返回并发送完响应。
- `Connection: close`：当前响应发完后优先关闭。
- 否则在最后一个响应发送完后，若 `peer_closed_ = true`，关闭连接。
- `isIdle()` 必须同时满足输入为空、输出为空、无 ready 响应、无 Worker 正在计算。Timer 只调用这个统一状态谓词，避免不同位置出现相反判断。

## Worker 的边界

Worker 只能执行 `SearchApplication::handleRequest()` 并产出 `ResponseResult`。不能直接操作 fd、Buffer、Channel 或 Timer：否则会产生缓冲区竞争、epoll 注册状态竞争、fd 关闭后复用，以及定时器状态竞争。

`SearchApplication` 在启动时构建索引，之后只读查询，因此当前阶段可以被多个 Worker 并发调用。

WorkerPool 在 `EventLoop` 之后创建，函数退出时会先析构并 `join()` Worker，再析构 EventLoop。这保证 Worker 的回传不会访问已销毁的 EventLoop。

## 验证

新增 `tcp_connection_async_request_test`：向同一 socket 一次写入 `/first` 与 `/second`。`/first` 的 Worker 故意延迟 30ms，`/second` 无延迟；预期接收字节严格是 `firstsecond`。这验证第二个请求不会绕过第一个请求提前处理或响应。

Linux Release 构建通过，`ctest` 共 14/14 通过。

## 下一步

进行端到端请求与基准压测，对比 WorkerPool 接入前后的吞吐、延迟、CPU 用户态/内核态占比。要用数据判断当前轻量内存检索是否值得承担线程调度成本。
