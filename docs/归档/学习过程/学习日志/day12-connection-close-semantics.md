# Day 12：Connection: close 协议语义

## 本日目标

让服务端理解客户端请求头 `Connection: close`，并在当前请求的完整 HTTP 响应发送后主动关闭连接。

## 为什么需要

Keep-Alive 优化后，服务端默认复用连接。但连接复用必须尊重客户端对连接生命周期的明确要求，否则会出现协议语义与实际 socket 状态不一致的问题。

## 设计

### 1. HTTP 层解析请求头

`HttpRequest` 新增 `headers`，`parseRequest()` 在请求行后按 `\r\n` 逐行读取，遇到空行结束。请求头字段名和值由第一个 `:` 分隔，并跳过值左侧的空格或 Tab。

请求头名和值的大小写无关判断封装为：

- `findHeader(request, name)`：按 HTTP 规则忽略字段名大小写查找。
- `shouldCloseConnection(request)`：识别 `Connection: close`。

当前仅支持值精确等于 `close`，不支持 `Connection: keep-alive, close` 这类多个 token 的完整语义，已作为后续协议完善项记录。

### 2. 业务层与网络层的边界

`SearchApplication` 不操作 fd，`TcpConnection` 不解析 HTTP 头。两者用结构化结果交接：

```cpp
struct ApplicationResponse {
    std::string response;
    bool close_after_response;
};
```

`main` 将它适配为 `TcpConnection::ResponseResult`。连接层只执行“发送完当前输出后是否关闭”的布尔命令。

### 3. 连接状态机

```text
解析当前请求为 Connection: close
-> 生成响应，响应头也写 Connection: close
-> close_after_response_ = true
-> 将 output_buffer_ 发送完整
-> handleWrite() 发现标记
-> removeChannel + close(fd)
```

若同一输入缓冲区中还存在后续请求字节，也不再处理，因为前一条请求已声明该响应完成后结束整个 TCP 连接。

## 验证

- Release 构建的 7 个测试全部通过。
- `HttpRequestTest` 覆盖无 query 的请求头解析、无空格 `Connection:close`、Tab 空白、字段名和值的大小写无关判断。
- `SearchApplicationTest` 验证 `Connection: close` 同时影响业务结果的 `close_after_response` 和实际 HTTP 响应头。
- 远程 18089 端口原始 TCP 验证：发送 `Connection: close` 后收到 HTTP 200、完整 JSON、`Connection: close` 响应头；持续读取在 5 秒超时前自然结束，证明服务端是在响应发送完成后关闭 socket。

## 真实问题记录

接口从 `std::string` 演进为 `ResponseResult` 时，`main` 中的回调一度仍返回旧类型。C++ 编译器在构建阶段指出 lambda 无法转换为新的 `RequestHandler`，避免了错误接口进入运行期。这说明跨层接口调整必须重新构建所有调用方，并由测试覆盖关键语义。

## 未完成边界

- Keep-Alive 空闲连接还没有超时回收机制，长期空闲连接会占用 fd。
- 未解析 `Connection` 的多个 token。
- 尚未支持 HTTP 请求体及 `Content-Length`。

## 下一步

设计空闲连接超时回收。它需要计时器与 EventLoop 协作，保证过期回调不会误关闭已经复用或已销毁的连接。
