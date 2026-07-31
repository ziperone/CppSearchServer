# Project Problem And Value

## The Real Problem

The project should not be described as:

> I implemented a C++ Reactor web server.

That is too generic and too common.

The project should be described as:

> I built a lightweight C++ HTTP search service for low-latency keyword search over local job/project documents.

The real problem is:

> During job preparation, project notes, interview notes, resumes, and backend learning documents are scattered across local files. Manually finding relevant content is slow and hard to reuse. A small local search service can index these documents and provide fast keyword search through an HTTP API.

This makes the project scenario concrete:

- User: a job seeker / developer.
- Data: local Markdown, text, or JSONL documents.
- Request: keyword query such as `epoll reactor`, `LRU cache`, `resume project`.
- Output: ranked matching documents/snippets.
- Value: quickly locate relevant project notes and interview material.

## Why Reactor Alone Is Not Enough

Only implementing Reactor is usually not enough for a strong resume project.

Reasons:

- Many candidates have done TinyWebServer-like projects.
- Reactor is infrastructure, not a complete user-facing service.
- If there is no business scenario, the project becomes "I used epoll" rather than "I solved a problem".
- Interviewers can easily classify it as a standard learning project.

Reactor is still valuable, but it should be the backend foundation, not the whole project.

Correct positioning:

```text
Reactor is the infrastructure layer.
Search service is the product layer.
Index and cache are the backend value layer.
Benchmark and docs are the proof layer.
```

## What Makes The Project More Valuable

The project becomes more defensible when it contains these layers:

### 1. Network Layer

Shows C++ backend fundamentals:

- socket
- nonblocking fd
- epoll
- EventLoop
- connection lifecycle

Interview value:

> Can explain how a C++ server handles many connections.

### 2. HTTP API Layer

Shows service interface ability:

- `/search?q=...`
- `/metrics`
- JSON response
- error response

Interview value:

> Not just a network toy; it exposes a usable service.

### 3. Search Layer

Shows a real backend function:

- document loader
- tokenizer
- inverted index
- TF-IDF or simple BM25
- snippets

Interview value:

> Solves a concrete information retrieval problem.

### 4. Cache Layer

Shows backend optimization thinking:

- LRU cache
- hot query reuse
- hit/miss metrics
- before/after comparison

Interview value:

> Can identify repeated query cost and optimize it with cache.

### 5. Measurement Layer

Shows engineering proof:

- benchmark script
- latency or throughput comparison
- cache hit ratio
- README demo

Interview value:

> Can prove the system works instead of only describing it.

## Minimum Valuable Version

For 30 days, the minimum valuable version should include:

- epoll EventLoop HTTP server.
- document loading from local files.
- `/search?q=...` endpoint.
- inverted index.
- simple ranking.
- LRU cache.
- `/metrics` or printed metrics.
- README with demo.
- benchmark report.

This is enough to avoid being "just a Reactor project".

## Interview Hook

Recommended hook:

> I used C++ to implement a lightweight local document search service. The system uses epoll EventLoop to handle concurrent HTTP requests, builds an inverted index over local job/project notes, and adds an LRU cache for repeated hot queries. I used benchmark scripts to compare cached and uncached query latency.

Why this hook works:

- It has a real scenario.
- It has C++ backend infrastructure.
- It has a searchable data model.
- It has an optimization point.
- It has measurable results.

## Project Value Judgment

### If Only Reactor Is Implemented

Value:

- Good for learning Linux network programming.
- Weak as a standout resume project.

Risk:

- Looks like a standard WebServer reproduction.
- Hard to prove user value.
- Interviewer may ask what is different from TinyWebServer.

Conclusion:

> Not enough.

### If Reactor + Search Is Implemented

Value:

- Has real service behavior.
- Can connect to job preparation scenario.
- Shows both network and backend business logic.

Conclusion:

> Acceptable GitHub project.

### If Reactor + Search + LRU Cache + Benchmark Is Implemented

Value:

- Has infrastructure.
- Has real problem.
- Has optimization.
- Has measurement.
- Can produce strong resume bullets.

Conclusion:

> Good 30-day C++ backend project.

## Final Scope Decision

The 30-day project should not stop at Reactor.

Final scope:

```text
epoll HTTP server
  + local document search
  + inverted index
  + LRU cache
  + benchmark/demo
```

This scope is small enough to finish in 30 days and strong enough to discuss in interviews.

