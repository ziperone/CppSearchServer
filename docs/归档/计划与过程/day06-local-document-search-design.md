# 本地求职文档检索设计

## 问题定义

为个人求职资料建立低延迟本地检索：简历、项目经历、面试复盘、理论知识和技能笔记应能被 `/search?q=...` 检索，并返回可引用的具体章节，而非仅返回整份文件。

## 设计原则

- 个人语料规模小，优先可解释、低依赖、易验证，不复刻大型搜索引擎。
- 优先 Markdown 和纯文本；PDF 作为后续预处理输入，不在 C++ 服务内直接解析。
- 以 Markdown 标题切分为 chunk，保留标题路径、分类和来源路径。
- 采用“原文短语匹配 + 技术词归一化 + 倒排索引”的轻量混合检索。
- 不先引入中文分词库；技术词和个人术语用自定义词典保护，中文自然语言先以短语/子串匹配兜底。

## 语料规范

```text
data/docs/
  resumes/
  projects/
  interviews/
  knowledge/
  skills/
```

首期只加载 `.md`、`.txt`。每个 Markdown 文档可以使用可选 front matter：

```text
title: CppSearchServer 项目复盘
category: projects
tags: [C++, epoll, Reactor]
---
```

没有 front matter 时，使用文件名和目录作为默认元数据。

## 核心数据模型

```text
DocumentChunk
  id
  source_path
  category
  title
  heading_path
  raw_text

Posting
  chunk_id
  term_frequency

InvertedIndex
  normalized_term -> [Posting]
```

## 建索引流程

```text
扫描文件
  -> 读取 Markdown/文本
  -> 按标题切分 DocumentChunk
  -> 归一化与 token 提取
  -> token -> chunk_id/tf 写入倒排索引
```

## 查询流程

```text
/search?q=epoll reactor
  -> HTTP 解析取得 q
  -> 查询归一化
  -> 原文短语匹配加分
  -> 倒排索引召回候选 chunk
  -> TF-IDF/简化 BM25 排序
  -> 标题、标签和分类加权
  -> 返回 topK 标题、来源、摘要与分数
```

## 分词策略

第一版：

- ASCII 技术词小写归一化，例如 `EPOLL` -> `epoll`。
- 保留 `C++`、`CMake`、`LRU`、`RAG`、`MySQL`、`Redis`、`EventLoop` 等术语。
- 中文查询优先原文子串/短语匹配；必要时对连续中文生成二元短语作为召回补充。

后续仅在中文自然语言召回明显不足时再评估引入 `cppjieba` 等第三方库。

## 阶段计划

1. 定义数据目录、`DocumentChunk`，加载 Markdown/文本并按标题切块。
2. 实现归一化和 tokenizer，建立可解释的倒排索引。
3. 实现 query 召回、排序、摘要和 `/search` JSON 响应。
4. 用真实求职材料构建 demo 数据集，加入正确性测试与 benchmark。

## 明确不做

- 首期不直接解析 PDF。
- 首期不接向量数据库、embedding 或 LLM。
- 首期不做通用中文 NLP、复杂布尔查询、权限系统或分布式索引。
