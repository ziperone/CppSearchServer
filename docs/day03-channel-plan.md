# 今日学习：Channel

## 今日目标

将当前 `EventLoop` 中分散的 `fd -> callback` 映射逐步演化为最小 `Channel` 模型，明确一个 fd 的关注事件与回调归属。

## 为什么现在学习 Channel

当前最小版本存在以下不足：

- `main.cpp` 直接注册 lambda，连接的事件、回调和生命周期分散。
- `EventLoop` 只保存读回调，难以自然支持写事件、错误事件和关闭事件。
- 异常断连时“取消 epoll 关注、删除回调、关闭 fd”的职责没有统一归属。
- 后续实现非阻塞 HTTP 需要保存未读完请求、未写完响应等连接状态；只靠一个 `int fd` 不够。

## Channel 的职责边界

- 一个 `Channel` 对应一个 fd。
- 保存该 fd 感兴趣的事件，以及读/写/错误等回调。
- 收到 epoll 就绪事件后，决定调用哪一个已注册回调。
- 不负责 HTTP 解析、搜索业务或线程池调度。
- 在后续阶段，连接对象负责 fd 的最终关闭和读写缓冲；Channel 负责事件分发。

## 今日范围

1. 画清 `Epoller -> EventLoop -> Channel -> callback` 的事件链路。
2. 设计最小 Channel 接口和成员变量。
3. 手写并检查 `Channel::handleEvent` 的简化核心逻辑。
4. 将 EventLoop 从“直接找 callback”改为“找到 Channel 并调用 handleEvent”。

## 完成标准

- 能解释 Channel 与 EventLoop 的职责差异。
- 能解释为什么 Channel 不应直接承担 HTTP 业务。
- 能手写简化的 `handleEvent`，区分 `EPOLLIN`、错误/挂断事件。
- 能说明当前阶段仍未解决哪些问题：半包、写缓冲、优雅退出、线程池。

## 本次实现与关键结论

- `EventLoop` 已从 `fd -> read_callback` 改为 `fd -> shared_ptr<Channel>`。
- `Channel::handleEvent(revents)` 是本阶段手写核心：错误/挂断优先，其次处理可读事件。
- `enableReading()` 只修改 Channel 的兴趣事件位（声明“未来关注可读”），不判断 fd 当前是否可读；`addChannel()` 随后通过 `epoll_ctl` 将该兴趣事件登记给内核。
- `dispatch()` 在调用 `handleEvent()` 前复制一个局部 `shared_ptr`。回调内若 `removeChannel(fd)`，map 中的所有权会删除，但该局部副本会让 Channel 活到当前 `handleEvent()` 返回，避免对象生命周期问题。

## 验证状态

- 已完成静态检查：旧的 `addReadEvent/removeEvent/read_callbacks_` 接口无残留，`Channel.cpp` 已加入 CMake 构建清单，`git diff --check` 无空白错误。
- 2026-08-03 Linux 构建通过：CMake 编译了 `main.cpp`、`Channel.cpp`、`EventLoop.cpp`、`Epoller.cpp`、`Socket.cpp` 和 `HttpResponse.cpp`，并成功链接 `cpp_search_server`。
- 2026-08-03 HTTP 验证通过：临时启动在 `18080` 端口；`GET /` 返回 `200` 和 `Hello CppSearchServer`，`GET /search?q=epoll` 返回 `200`。验证后已停止临时进程。

## 发现的真实问题：query 参数尚未解析

### 现象

请求 `/search?q=epoll` 的 JSON 响应中，`query` 字段为 `q=epoll`，而不是期望的 `epoll`。

### 原因

当前 `extractPath()` 只从 HTTP 请求行取得 `/search?q=epoll` 这一完整 target；`route()` 仅删除 `/search?` 前缀，没有将 query string 拆成键值对。

### 后续修复计划

- 在 HTTP 模块实现 request-target/query 参数解析。
- 明确只接受 `q` 参数，处理缺失、空值、多个参数和 URL 编码的边界。
- 添加 `/search?q=epoll` 返回 `query: epoll` 的回归验证。

### 面试价值

该问题来自真实 HTTP 验证而非凭空假设，能够说明项目通过端到端测试持续发现并缩小协议层边界问题。
