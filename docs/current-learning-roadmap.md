# CppSearchServer 当前学习路线图

## 项目主线

本项目不是“复现一个 Reactor WebServer”，而是解决一个具体问题：

> 为求职资料、项目笔记、面试题和技能文档提供低延迟本地关键词检索 HTTP 服务。

它未来可作为 Agent/RAG 的关键词证据检索分支，但当前 GitHub 版本首先是一项独立、可运行、可压测的 C++ 后端项目。

```text
Reactor 网络层
-> HTTP API
-> 本地文档加载与倒排索引
-> 排序与 JSON 响应
-> Keep-Alive 与连接治理
-> Worker 异步计算
-> LRU 热查询缓存与指标
-> 压测、README、面试表达
```

## 已完成：从网络连接到连接治理

### 1. 阻塞式服务器与 TCP 基础

完成内容：`socket`、`bind`、`listen`、`accept`、`recv`、`send`、`writeAll`。

为什么做：先理解一个 HTTP 请求从端口进入、绑定 clientfd、读取字节流、返回响应的完整闭环，才能理解后续并发优化究竟改了什么。

当前掌握：`listenfd` 监听新连接；`clientfd` 表示单条 TCP 连接；fd 是内核资源句柄，不存业务数据本身。

### 2. epoll Reactor、EventLoop 与 Channel

完成内容：非阻塞 fd、`epoll_ctl`、`epoll_wait`、`Channel`、`EventLoop`。

为什么做：阻塞式逐连接处理会被一个慢连接卡住。epoll 只返回就绪 fd，EventLoop 按 Channel 分发读写回调，实现并发调度但不等同于多核并行。

当前掌握：内核只返回就绪事件，不执行 C++ 回调；EventLoop 根据 Channel 执行回调。`EPOLLOUT` 仅在 `send()` 遇到 `EAGAIN` 且仍有待发送数据时临时关注。

### 3. 连接生命周期与 Keep-Alive

完成内容：输入/输出缓冲区、半关闭处理、流水请求顺序、`Connection: keep-alive`、`Connection: close`。

为什么做：真实 TCP 数据可能分片、响应可能部分发送，连接不能因为对端半关闭或一个请求处理结束就错误丢弃数据。Keep-Alive 直接降低短连接的建连/关闭内核态成本。

验证结果：在相同 `wrk -t4 -c64` 回环负载下，Keep-Alive QPS 从约 49,815 提升到 97,508，P99 从 1.58ms 降到 1.27ms，`%system` 从 63.25% 降到 36.80%。

### 4. HTTP 与本地检索服务

完成内容：请求行、query string、URL 解码、请求头解析、`/search?q=`、Markdown/TXT 切块、Tokenizer、倒排索引、并集召回、TF-IDF 排序、TopK JSON 响应。

为什么做：Reactor 是基础设施，检索服务才是项目解决的真实业务问题。索引在启动期构建、查询期常驻内存，避免每次请求遍历文档。

当前掌握：HTTP 请求头表达本次请求属性；`Connection: close` 必须在本次完整响应发送后关闭，而不是收到请求立即关闭。

### 5. 性能基线与连接超时治理

完成内容：Release 构建、wrk 压测、pidstat 资源监控、TimerQueue、EventLoop 定时器、单连接单 Timer 空闲超时回收。

为什么做：优化必须有数据基线。Keep-Alive 之后还需要治理长期无请求连接，否则 fd 会被持续占用。

当前设计：

```text
TimerQueue：最小堆，只管理到期时间和回调
EventLoop：计算 epoll_wait(timeout)，先处理 fd，再处理到期 Timer
TcpConnection：last_activity_at + generation + isIdle 决定是否关闭
```

关键优化：活动时只更新 `last_activity_at_` 和 `idle_generation_`，不创建新 Timer；唯一旧 Timer 到期后才按最后活动时间安排下一次检查，避免高 QPS 下大量无效 Timer 堆积。

验证：Release 11 项测试通过，包含空闲关闭与活动后延迟关闭的真实 `socketpair` 集成测试。

## 当前所在位置

已完成单线程高并发 I/O 调度、HTTP 连接语义、内存检索和连接治理。

下一项不是“直接让 Worker 写 socket”，而是先建立跨线程任务回传机制：Worker 只做计算，所属 EventLoop 独占连接状态。

## 后续主线：Day 14-30

### 阶段 A：Worker 异步回传与线程池（Day 14-17）

1. **eventfd + PendingTaskQueue**：Worker 将完成结果放入线程安全队列，写 eventfd 唤醒 `epoll_wait`。
2. **最小 WorkerPool**：Worker 只执行检索、排序、JSON 构造；不直接修改 fd、Buffer、Channel 或 TimerQueue。
3. **连接处理状态与任务版本**：`processing_request` 表示连接正在等待 Worker，避免 idle timeout 误关闭；Worker 回传时检查连接和任务版本仍有效。
4. **请求处理超时**：区分 Keep-Alive 空闲超时和 Worker 慢任务超时。

必要性：单 EventLoop 仍串行执行业务处理。线程池的价值是卸载可能变慢的检索/JSON 工作，让 I/O 调度保持及时；难点是连接状态所有权，而不是创建线程本身。

### 阶段 B：本地 LRU 缓存与可观测性（Day 18-21）

1. 设计并实现 `unordered_map + list` 的固定容量 LRU。
2. 查询路径改为：LRU -> 内存索引 -> 填充 LRU -> 响应。
3. 增加请求数、缓存命中/未命中、查询耗时等指标；提供轻量 `/metrics`。
4. 使用相同负载对比缓存前后 QPS、P50/P99 与命中率。

必要性：检索服务的典型优化是处理热点查询。缓存提供一个可量化的后端优化点，也为 Redis 二级缓存建立清晰边界。

### 阶段 C：可靠性、压测与项目表达（Day 22-26）

1. 补充错误路径、边界请求、连接关闭和超时测试。
2. 扩展求职资料样例文档，展示真实查询场景。
3. 固定压测矩阵：短连接/Keep-Alive、单线程/WorkerPool、缓存命中/未命中。
4. 记录资源数据：QPS、P50/P99、`%usr/%system`、RSS、错误率。

必要性：简历项目的说服力来自“发现瓶颈 -> 单点优化 -> 同场景复测”的证据链，而不是模块数量。

### 阶段 D：GitHub 与面试交付（Day 27-30）

1. 中文 README：问题、架构、构建运行、curl 示例、压测结果、局限与未来扩展。
2. 架构图与模块边界说明。
3. 面试问答：epoll、Buffer、Keep-Alive、TimerQueue、WorkerPool、LRU、压测结论。
4. 简历表述：C++ 后端版、检索/RAG 基础设施版、缓存/存储版。

必要性：面试官需要能快速运行、理解并追问；用户需要能用自己的语言解释每个关键取舍。

## 明确延后项

- 写超时、完整 HTTP 规范、HTTP 请求体。
- Redis、MySQL 实现：先完成 LRU、指标和基线后，再以可选扩展实现或设计文档呈现。
- 分布式部署：只做架构设计，不在 30 天核心版本中堆叠实现。
- 中文分词、向量检索：保留给 Agent/RAG 系统，与当前关键词检索形成混合召回扩展。

## 每日学习规则

1. 先回顾上一模块与一个核心问题。
2. 开始前说明“当前缺口、为什么现在做、完成标准”。
3. 仅手写网络状态机、生命周期和性能关键逻辑；模板代码、字符串细节和重复样板由助手实现并解释。
4. 每阶段用测试或压测验证，并以中文 Git 提交。
5. 结束时用费曼式流程复述检查逻辑掌握，而不是背代码。
