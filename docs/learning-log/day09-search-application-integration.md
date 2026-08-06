# Day 09：检索服务端到端接入

## 本日目标

将文档加载、倒排索引和 `SearchService` 接入 Reactor HTTP 服务，使 `/search?q=...` 返回真实 JSON 证据，而不是占位响应。

## 应用生命周期

`SearchApplication` 按成员顺序持有：

```text
chunks_ -> index_ -> search_service_
```

服务启动时加载 `data/docs`、按标题切分为 chunks、构建倒排索引。`SearchService` 引用稳定存在的 chunks 和 index，仅在请求期执行只读内存查询。

`main` 中的 `SearchApplication` 比 `EventLoop` 存活更久。EventLoop 的连接回调通过它处理 HTTP 请求，因此该对象不能在事件循环运行期间销毁。

## 请求链路

```text
客户端请求
-> EventLoop 调度可读 client fd
-> TcpConnection 读取并拼接完整 HTTP 请求
-> SearchApplication::handleRequest
-> HttpRequest 解析并 URL 解码 q
-> SearchService 查询内存索引、返回 TopK
-> SearchApplication 序列化 JSON
-> TcpConnection 输出缓冲与 send 返回客户端
```

请求期不会再次执行文件扫描、Markdown 切块或索引构建。

## URL 解码

URL 编码属于 `HttpRequest` 协议解析层，而非 `SearchService`：业务检索层只接收普通查询文本，其他接口也能复用同一份解析规则。

- `%20` 解码为空格。
- `%2B` 解码为 `+`。
- 查询参数中的 `+` 解码为空格，因此 C++ 应以 `C%2B%2B` 传递。
- `%ZZ`、末尾残缺 `%` 等非法编码使 HTTP 请求解析失败，返回 `400`。

## 真实问题与修复

构建时发现监听回调没有捕获 `request_handler`，导致无法将业务处理器传给新建 `TcpConnection`。修复为按值捕获：监听回调拥有处理器副本，每条连接再保存自己的回调副本，避免回调上下文缺失。

## 验证结果

- Linux CMake 构建成功。
- 7 项单元测试通过：包含 HTTP 请求解析、应用组装、URL 解码和非法编码拒绝。
- 真实 TCP 请求 `q=epoll` 返回可引用 chunk JSON。
- 真实 TCP 请求 `q=epoll+reactor` 解码后返回 `matched_terms: 2`。

## 下一步

建立可重复的性能基线：准备不同规模文档集，定义压测负载，记录 QPS、P50/P95/P99、错误率与内存占用。后续所有缓存、线程池和 Redis 优化均以基线数据验证。
