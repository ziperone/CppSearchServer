# Day 06：本地文档切块

## 本日目标

让个人求职资料从“文件集合”变为可引用、可排序的检索单元，为后续倒排索引和 `/search` 真实结果提供输入。

## 已完成

- 新增 `search::DocumentChunk`：保存 `id`、来源路径、分类、文档标题、标题路径和正文。
- 新增 `DocumentLoader`：递归读取 `data/docs` 中的 `.md` 与 `.txt` 文件。
- 手写 `chunkMarkdown` 核心状态机：按 Markdown 标题切块。
- 新增 Markdown 切分测试与文档加载测试，均已在 Linux 环境通过。

## 核心理解

### 为什么按标题切块

检索结果应返回与查询直接相关的小节，而不是整篇简历或项目复盘。这样可以：

- 对不同小节独立打分和排序。
- 返回简短摘要、来源文件和具体标题路径。
- 为后续 Agent/RAG 提供可追溯的证据片段，避免将整份笔记直接塞入上下文。

### `chunkMarkdown` 的状态机

函数逐行读取文档，并维护：

- `headings`：当前标题栈，例如 `项目 > 网络模型 > epoll`。
- `current_body`：当前标题下尚未提交的正文。
- `document_title`：一级标题。

读到普通行时追加到 `current_body`。读到新标题时，先 `flushCurrent()` 提交旧标题对应正文，再按照新标题级别裁剪标题栈并压入新标题。

新标题为 `##` 时，保留一级标题，删除旧二级标题及更深标题；这避免路径错误地残留旧分支。循环结束后还需最后一次 `flushCurrent()`，因为最后一个标题没有下一个标题来触发提交。

### 无标题文本

`.txt` 没有 Markdown 标题时也不能丢弃。首版将它作为一个完整 chunk，标题取文件名；其中仍可能有技能笔记、面试结论等可检索信息。

## 验证结果

Linux CMake 构建成功，`ctest --output-on-failure`：

- `markdown_chunker_test` 通过：覆盖标题路径、正文归属、连续 id、元数据与无标题文本兜底。
- `document_loader_test` 通过：覆盖 `data/docs` 的递归扫描与样例 Markdown 切块。

## 下一步

为 `DocumentChunk` 建立轻量 tokenizer 和倒排索引：`term -> [chunk_id, term_frequency]`。
