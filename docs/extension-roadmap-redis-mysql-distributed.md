# Extension Roadmap: Redis, MySQL, Distributed Design

## Feasibility Judgment

The idea is feasible:

> Use the current C++ search service as the core backend project, then extend it with Redis, MySQL, and distributed-system design notes to support different resume directions.

But the implementation order matters.

Correct priority:

```text
search service works first
  -> local LRU cache
  -> benchmark
  -> Redis/MySQL extension boundaries
  -> optional prototype
```

Wrong priority:

```text
Redis + MySQL + distributed design before search works
```

That would make the 30-day project too scattered.

## Project Ecosystem Position

This C++ project can be the retrieval infrastructure for a later job-interview Agent system.

In that ecosystem:

```text
C++ Search Server:
  fast keyword retrieval
  exact term matching
  local index
  cache
  HTTP API

Agent/RAG System:
  JD analysis
  retrieval tool invocation
  prompt construction
  answer/resume generation
  interview question generation
```

Relation:

```text
C++ Search finds evidence.
Agent/RAG generates answers based on evidence.
```

## Why Not Just Use Vector RAG

Vector RAG is useful, but keyword search still matters:

- exact terms like `epoll`, `Reactor`, `LRU`, `MVCC`, `C++11` need precise matching.
- keyword search is explainable.
- local C++ search has low latency and no token/embedding cost.
- it can serve as the keyword branch of a future hybrid retrieval system.

Future hybrid retrieval:

```text
query
  -> C++ keyword search
  -> vector search
  -> merge/rerank
  -> Agent answer generation
```

## Redis Extension

### Role

Redis should be positioned as second-level shared cache.

Cache flow:

```text
request query
  -> local LRU cache
  -> Redis cache
  -> SearchEngine
  -> fill Redis
  -> fill local LRU
  -> response
```

### What Redis Demonstrates

- two-level cache design.
- hot query optimization.
- shared cache across multiple server instances.
- TTL and invalidation discussion.
- cache penetration/breakdown/avalanche knowledge.

### 30-Day Boundary

Must document:

- where Redis fits.
- key/value format.
- cache hit/miss flow.

Optional prototype:

- only after local LRU and search endpoint are stable.

## MySQL Extension

### Role

MySQL should store metadata, not necessarily full search index in the 30-day version.

Possible table:

```sql
documents(
  id BIGINT PRIMARY KEY,
  title VARCHAR(255),
  source_path VARCHAR(512),
  tags VARCHAR(255),
  updated_at DATETIME
)
```

The inverted index can remain in memory/local files.

### What MySQL Demonstrates

- metadata persistence.
- index design.
- SQL query basics.
- transaction/MVCC interview preparation.
- separation between metadata store and search index.

### 30-Day Boundary

Must document:

- what data belongs in MySQL.
- why inverted index is not stored in MySQL for this version.

Optional prototype:

- a small `DocumentStore` interface and mock/local implementation.

## Distributed Design Extension

Do not build a real distributed system in 30 days.

But document the future design:

```text
client/Agent
  -> load balancer
  -> multiple C++ SearchServer instances
  -> Redis shared cache
  -> MySQL metadata store
  -> local/prebuilt index files
```

What this demonstrates:

- stateless service thinking.
- shared cache.
- metadata persistence.
- horizontal scaling awareness.
- cache consistency discussion.

## Resume Strategy

### For C++ Backend Roles

Emphasize:

- epoll EventLoop.
- nonblocking socket.
- connection lifecycle.
- HTTP service.
- benchmark.

### For Backend Storage/Infrastructure Roles

Emphasize:

- inverted index.
- LRU cache.
- Redis second-level cache design.
- MySQL metadata design.
- metrics and benchmark.

### For AI/Agent Infrastructure Roles

Emphasize:

- retrieval backend for Agent/RAG.
- keyword exact matching.
- HTTP tool API.
- evidence retrieval.
- future hybrid retrieval.

## Final Decision

This extension path is feasible, but Redis/MySQL/distributed content should be treated as:

```text
core project extension and interview depth
```

not:

```text
mandatory dependency for the 30-day GitHub demo
```

