# Day 07：Tokenizer 与倒排索引

## 本日目标

让服务可以从查询词直接定位候选文档块，而不是每次查询都扫描所有全文。

## 已完成

- 新增 `tokenize`：统一英文技术词的大小写和切分规则。
- 新增 `InvertedIndex` 与 `Posting`。
- 手写 `InvertedIndex::addChunk` 的核心索引构建逻辑。
- 新增 tokenizer 与倒排索引单元测试。

## 为什么需要倒排索引

正向文档结构是 `chunk -> terms`，它适合存储正文，但查询一个词时需要扫描所有 chunk。

倒排索引反转关系：

```text
term -> [{chunk_id, term_frequency}, ...]
```

例如 `epoll` 可以直接定位所有出现它的 chunk，并保留该词在各 chunk 内的词频，为后续排序提供依据。

## 核心实现

`addChunk` 分两步：

1. 对当前单个 chunk 的 tokens 建立局部 `term_frequencies`。
2. 每个 term 只向全局 `postings_by_term_` 写入一条 `Posting{chunk.id, frequency}`。

局部词频不能做成全局变量，因为它只表达一个 chunk 内的频率。若跨 chunk 累积，`chunk 7` 的 `epoll=2` 与 `chunk 8` 的 `epoll=1` 会混成 3，丢失可排序、可定位的证据信息。

## 查询缺失词

`findTerm` 返回 `const std::vector<Posting>*`：

- 词不存在时返回 `nullptr`，表示没有命中 chunk。
- 避免复制可能很长的 posting list。
- `const` 防止查询方修改索引内部数据。

最终搜索服务遇到 `nullptr` 应返回空结果集合；HTTP 层再将其表示为 JSON 空数组，而非空字符串。

## 验证结果

Linux CMake 构建成功，`ctest --output-on-failure` 的 4 项测试均通过：

- `markdown_chunker_test`
- `document_loader_test`
- `tokenizer_test`
- `inverted_index_test`

## 下一步

实现多关键词查询的候选合并、基础相关性评分和搜索结果对象，将 `/search?q=...` 从占位响应接入真实索引。
