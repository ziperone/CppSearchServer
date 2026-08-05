# Day 08：多关键词召回与排序

## 本日目标

将单词倒排索引升级为可处理多关键词的搜索服务，并在不过度复杂化的前提下返回相关度最高的文档证据。

## 排序逻辑

查询首先经过与建索引完全相同的 `analyzeTerms`：技术词归一、英文停用词过滤。随后对查询词去重，避免 `EPOLL epoll` 将同一概念重复计分。

对于每个不同查询词：

1. 从倒排索引取得 posting list。
2. 对其中每个 `chunk_id` 建立或更新候选项。
3. `matched_terms` 加一，表示该 chunk 命中了一个不同的查询词。
4. 累加轻量 TF-IDF 分数。

候选集是各查询词 posting list 的并集，而不是严格交集。并集保证部分匹配的求职资料仍能被返回；`matched_terms` 让同时覆盖更多查询概念的 chunk 排在前面。

比较顺序固定为：

1. `matched_terms` 降序。
2. TF-IDF `score` 降序。
3. `chunk_id` 升序，作为稳定的确定性兜底。

例如查询 `epoll reactor`：

```text
chunk A: epoll 出现 10 次，reactor 未出现 -> coverage 1
chunk B: epoll 与 reactor 各出现 1 次     -> coverage 2
```

chunk B 优先，因为它覆盖了用户查询中的两个不同概念；词频只能在 coverage 相同时作为次级排序信号。

## TF-IDF

```text
idf = log((document_count + 1) / (document_frequency + 1)) + 1
score += log(1 + term_frequency) * idf
```

- `term_frequency` 是词在当前 chunk 中的出现次数。
- `document_frequency` 是包含该词的 chunk 数，即 posting list 长度。
- 对 TF 取对数，防止单个词重复出现无限抬高分数。
- 稀有技术词的 IDF 更高，具有更强区分度。

## 性能取舍

服务只遍历查询词对应的 posting list，不扫描所有正文。候选结果超过 `top_k` 时，使用 `std::partial_sort` 只选出并排序前 K 个，避免对全部候选做完整排序；其成本约为 `O(candidate_count * log(top_k))`。

对比完整 `std::sort` 的 `O(candidate_count * log(candidate_count))`，当候选数远大于 K 时，TopK 选择的排序成本更低。

`std::partial_sort` 的语义是：前 K 个元素为全局最优且内部有序，后续元素的顺序不作要求。主流实现通常采用堆式 TopK 选择：维护容量为 K 的候选堆，扫描其余候选，仅当新候选优于当前第 K 名时才替换并调整堆，最后排序 K 个结果。C++ 标准不强制具体内部算法，但保证该函数的 TopK 语义与相应复杂度边界。

## 当前边界

- 使用 OR 召回加覆盖度排序，不提供严格 AND 查询语法。
- 中文仍未进入 token 倒排索引，后续由短语匹配补充。
- 不做拼写纠正、SimHash 或 BM25，等待 benchmark 显示必要性后再评估。
