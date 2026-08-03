# 下一模块：TcpConnection 与 Buffer

## 要解决的问题

当前 `handleClient` 是最小演示路径：一次 `recv`、同步解析、一次 `writeAll`、立即关闭。客户端 fd 已经设为非阻塞，因此该实现无法正确覆盖真实连接生命周期。

## 当前局限

- 请求可能分多次到达；一次 `recv` 不一定读到完整 HTTP 请求。
- `recv` 返回 `EAGAIN` 表示暂时没有数据，不等于连接关闭或错误。
- 非阻塞 `send` 可能只写入部分响应，或返回 `EAGAIN`；当前 `writeAll` 不能保留剩余数据。
- 读、写、异常和关闭逻辑分散，`Channel` 只负责事件分发，无法保存连接业务状态。

## 本阶段目标

引入最小 `TcpConnection` 与读写 Buffer，使单个连接支持：

```text
建立连接
  -> 持续读取并累积请求字节
  -> 请求完整后生成响应
  -> 尽量发送响应
  -> 未发送完则临时关注 EPOLLOUT
  -> EPOLLOUT 到来后继续发送
  -> 响应全部发送后关闭连接
```

## 范围控制

- 支持单连接单请求单响应，响应完成后关闭。
- 使用 HTTP 头结束标记 `\r\n\r\n` 判断最小请求完整性。
- 不实现 keep-alive、完整 HTTP parser、请求体、线程池。
- 继续保留后续 query 参数解析作为 HTTP 模块任务。

## 新职责

| 组件 | 职责 |
|---|---|
| `Buffer` | 保存尚未解析的输入字节或尚未发送的输出字节。 |
| `Channel` | 保存关注事件并分发读/写/错误回调。 |
| `TcpConnection` | 管理一个客户端连接的状态、Buffer、Channel 和关闭时机。 |
| `EventLoop` | 管理 Channel 并调用其 `handleEvent`。 |

## 核心手写点

- `Buffer` 的追加与已消费字节移除逻辑。
- `TcpConnection::handleRead` 中“循环读取直到 EAGAIN”的分支判断。
- `TcpConnection::handleWrite` 中“部分发送、开启/关闭 EPOLLOUT”的分支判断。
- `TcpConnection::close` 的清理顺序。

## 完成标准

- 用分两次发送 HTTP 请求的测试证明输入缓冲有效。
- 用大响应或受限发送缓冲测试证明输出缓冲和 `EPOLLOUT` 有效。
- 在异常关闭时，不遗留 Channel 映射或打开的 client fd。

## 验证记录与发现

### 已通过

- Linux 编译已包含 `Buffer.cpp`、`TcpConnection.cpp` 及 Channel 写事件接口。
- 普通 `GET /` 返回 `200` 和预期响应。
- 分两段发送同一条 HTTP 请求：第一段不包含 `\r\n\r\n`，第二段补齐后服务返回 `200`，证明输入 Buffer 在请求边界前不会误路由。

### 发现：客户端半关闭会丢弃已接收请求

#### 复现

客户端发送完整 HTTP 请求后调用 `shutdown(SHUT_WR)`，仍等待服务器响应；当前服务返回空响应。

#### 原因

`TcpConnection::handleRead` 中 `recv == 0` 直接执行 `close()`。`0` 表示对端不再发送字节，不代表服务器不能继续向对端发送已经准备好的响应。

#### 修复方向

- 若 `recv == 0` 时输入 Buffer 已含完整请求且尚未生成响应，先调用 `processRequest()`。
- 响应的发送与最终关闭仍由 `handleWrite()` 决定。
- 若请求不完整，则关闭连接，避免等待永远不会到来的后续字节。

#### 面试价值

该问题来自主动设计的 TCP 半关闭测试，体现对“读方向关闭”和“整个连接关闭”不同语义的理解。

### 半关闭修复验证

- 修复后重新完成 Linux 构建。
- 使用同一复现方式：发送完整请求后 `shutdown(SHUT_WR)`。
- 服务返回完整 `HTTP/1.1 200 OK` 和响应正文，证明读方向 EOF 不会再提前打断写方向。

### 尚待强制验证：EPOLLOUT 分支

当前 `/` 响应很小，通常一次 `send` 即可写入内核发送缓冲区。因此普通和分段请求已经覆盖 Buffer 与基本写路径，但还没有强制产生 `EAGAIN`，尚未端到端证明 `EPOLLOUT -> handleWrite` 的恢复发送分支。后续应通过大响应或受限发送缓冲区测试该路径。

该验证暂留为 benchmark/test harness 阶段的 hook：稳定触发需要测试专用大响应或临时缩小 socket 发送缓冲区，不纳入当前服务的业务接口。

## 费曼总结

- `input_buffer_` 保存尚未被 HTTP 解析器消费的请求字节；`output_buffer_` 保存尚未写入内核发送缓冲区的响应字节。
- `recv` 的 `EAGAIN` 表示接收缓冲区当前为空，等待客户端未来发送字节并触发 `EPOLLIN`；`send` 的 `EAGAIN` 表示发送缓冲区当前没有空间，保留输出 Buffer 并临时关注 `EPOLLOUT`。
- `recv == 0` 只说明对端不再发送，不能无条件关闭整个连接。若已有完整请求，服务器仍应生成并发送响应；响应已生成时由 `handleWrite` 继续发送，输出 Buffer 清空后再关闭；请求不完整时才直接关闭。
