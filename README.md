# CppSearchServer

CppSearchServer 是一个面向 C++ 后端方向的学习型项目，目标是在 30 天内实现一个可运行、可展示、可解释的本地文档搜索 HTTP 服务。

项目不是单纯复现 WebServer，而是围绕一个具体小问题展开：

> 求职准备过程中，项目笔记、面试复盘、简历材料和 C++ 后端学习文档分散在本地文件中，手动查找效率低。CppSearchServer 计划通过 C++ 后端服务对这些本地文档建立索引，并提供 `/search?q=...` HTTP 接口进行低延迟关键词检索。

## 项目定位

核心定位：

```text
epoll HTTP 服务
  + 本地文档检索
  + 倒排索引
  + LRU 查询缓存
  + benchmark / 面试复盘文档
```

后续可以作为求职面试 Agent / RAG 系统的关键词检索后端：

```text
C++ Search Server 负责找证据；
Agent / RAG 负责基于证据生成回答、简历建议和面试追问。
```

## 当前阶段

当前已经完成：

- 最小阻塞式 HTTP Server。
- `socket -> bind -> listen -> accept -> recv -> send` 主流程学习。
- `Epoller` 封装：`epoll_create1 / epoll_ctl / epoll_wait`。
- `EventLoop` 初版：基于 epoll 的 fd 就绪事件分发。
- 本地学习文档与 30 天 GitHub 交付计划。

当前正在推进：

- 将阻塞式 `serveForever` 逐步升级为 epoll EventLoop 模型。
- 后续进入 `Channel`、HTTP query 解析、文档加载、倒排索引和 LRU 缓存。

## 技术栈

- C++17
- Linux socket
- nonblocking IO
- epoll
- EventLoop / Reactor
- HTTP 基础解析
- 倒排索引
- LRU cache
- CMake

## 构建与运行

Linux 环境：

```bash
cmake -S . -B build
cmake --build build
./build/cpp_search_server 8080
```

测试：

```bash
curl http://127.0.0.1:8080/
curl "http://127.0.0.1:8080/search?q=epoll"
```

## 计划亮点

最终项目希望体现：

- C++ 后端网络编程能力：socket、epoll、非阻塞 IO、事件循环。
- 服务化能力：HTTP API、路由、JSON 响应、错误处理。
- 检索能力：本地文档加载、Tokenizer、倒排索引、基础排序。
- 缓存优化：本地 LRU 缓存，后续可扩展 Redis 二级缓存。
- 工程证明：README、benchmark、面试追问文档、阶段性 Git 提交记录。

## 文档索引

- `docs/30-day-github-plan.md`：30 天 GitHub 交付计划。
- `docs/project-problem-and-value.md`：项目问题定义与含金量判断。
- `docs/extension-roadmap-redis-mysql-distributed.md`：Redis / MySQL / 分布式扩展路线。
- `docs/week01-mastery-checklist.md`：第一阶段阻塞式服务器掌握清单。
- `docs/week02-epoll-reactor-plan.md`：第二阶段 epoll Reactor 计划。
- `docs/functions/`：核心函数学习与费曼复盘。

## 学习规则

每个阶段遵循：

1. 先明确模块要解决什么问题。
2. 再实现或手敲核心函数。
3. 完成后进行复盘。
4. 用费曼法解释一遍。
5. 阶段完成后提交一次 GitHub，保留项目演进路径。

