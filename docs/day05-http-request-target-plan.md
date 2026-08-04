# 今日学习：HTTP Request Target 与 Query 参数解析

## 为什么现在学习

网络层已经能稳定接收分段字节、识别最小 HTTP 请求边界并生成响应，但业务入口仍把 `/search?q=epoll` 整体当成路径，响应中的 query 为 `q=epoll`。这说明 TCP 字节流已经正确到达 HTTP 层，但 HTTP request-target 还没有被结构化解析。

## 今日目标

- 将 HTTP 请求行拆为 method、path、query string。
- 将 query string 拆为键值参数，当前只消费 `q`。
- 让 `/search?q=epoll` 返回 `query: epoll`。
- 覆盖缺失 `q`、空 `q`、多个参数等基础边界。

## 范围控制

- 只支持 GET 和最小 request-target 解析。
- 不实现完整 URL 编码、请求体、keep-alive 或完整 HTTP parser。
- 保留未来扩展：参数 URL 解码、统一 Request 对象、错误响应。

## 完成标准

- Linux 构建通过。
- `/search?q=epoll` 返回解析后的 `epoll`。
- `/search?lang=cpp&q=epoll` 可正确提取 `q`。
- 缺少或为空的 `q` 返回明确的客户端错误响应。

## 实现与验证

- 新增 `HttpRequest`：保存 `method`、`path` 与 query 参数表。
- `parseRequest()` 解析 HTTP 第一行，并将 request-target 拆为 path 与 query string。
- `parseQueryString()` 按 `&` 和 `=` 提取参数；当前策略为忽略没有 `=` 的项，并保留首次出现的同名参数。
- `/search?q=epoll` 返回 `200`，JSON 中 `query` 为 `epoll`。
- 原始 HTTP 请求 `GET /search?lang=cpp&q=epoll HTTP/1.1` 返回 `200`，正确提取第二个参数 `q=epoll`。
- `/search?lang=cpp` 与 `/search?q=` 返回 `400 Bad Request`。

## 测试经验

远程 shell 中 `&` 具有后台执行语义，直接使用 shell 命令测试带多个 query 参数的 URL 时必须正确引用或转义。首次多个参数测试失败来自测试命令转义，而非 C++ 解析逻辑；通过直接发送原始 HTTP 请求完成确认。

## 当前边界

- 仅支持最小 GET request line 解析。
- 不处理 URL percent-decoding、重复参数语义、请求体或 JSON 字符串转义。

## 费曼总结

- TCP/Buffer 层只负责累积字节；`TcpConnection` 在发现 `\r\n\r\n` 后取出一条完整请求交给 HTTP 层。
- `parseRequest()` 从请求行取得 method 与 request-target，再拆出 path 和 query string；`parseQueryString()` 用 `&`、`=` 构建参数表。
- 路由先根据 `path=/search` 选择搜索业务，再读取 `q` 参数，而不是由 q 决定路由。
- `q=event%20loop` 当前会得到字面量 `event%20loop`。`%20` 是 URL percent-encoding 对空格的表示，必须显式 URL 解码后才能作为 `event loop` 搜索。
