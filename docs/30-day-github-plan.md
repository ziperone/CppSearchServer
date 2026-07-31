# 30-Day GitHub Plan

## Goal

Within 30 days, build a GitHub-ready C++ backend project:

> A lightweight high-performance document search HTTP server based on epoll Reactor.

The project should be suitable for C++ backend interviews and must have at least one clear interview hook.

## Interview Hook

The chosen hook:

> Solving the small but real problem of low-latency keyword search over local job/project documents through an epoll-based HTTP search service with inverted index and LRU query cache.

Detailed problem/value judgment:

- `docs/project-problem-and-value.md`

This is intentionally not a huge distributed system. The value is:

- It has a real scenario: search local documents through an HTTP service.
- It shows C++ backend basics: socket, epoll, nonblocking IO, event loop, HTTP.
- It shows backend engineering: index, cache, benchmark, README.
- It can be run and tested by an interviewer.

## Final GitHub Deliverables

By Day 30, the repository should contain:

- Working C++ server.
- `README.md` with project background, architecture, build/run commands, and demo.
- epoll-based EventLoop.
- HTTP `/search?q=...` endpoint.
- local document loading.
- simple tokenizer.
- inverted index.
- basic ranking: TF-IDF or simplified BM25.
- LRU cache for hot queries.
- benchmark/demo report.
- interview notes explaining key design decisions.

## Scope Control

Must do:

- epoll EventLoop.
- HTTP request path/query parsing.
- document search endpoint.
- inverted index.
- LRU cache.
- README and demo.
- Redis/MySQL/distributed-system study notes as optional resume expansion.

Optional only if time remains:

- thread pool.
- BM25 instead of TF-IDF.
- JSON library integration.
- keep-alive.
- graceful shutdown.
- Redis second-level cache prototype.
- MySQL document metadata storage prototype.
- distributed cache/service notes.

Do not do in this 30-day version:

- distributed system.
- searchable encryption.
- full HTTP parser.
- complex Chinese tokenizer.
- RPC framework.

Clarification:

Redis and MySQL can be added as controlled extensions, but the 30-day GitHub version must not depend on them to run. The core demo should still work with local files and in-memory LRU cache.

## Resume Positioning Variants

The same project can support different resume versions:

### C++ Backend Version

Emphasis:

- epoll EventLoop
- nonblocking IO
- connection lifecycle
- HTTP service
- benchmark

### Search/RAG Infrastructure Version

Emphasis:

- local document search
- inverted index
- ranking
- Agent/RAG retrieval backend
- evidence retrieval API

### Backend Storage/Cache Version

Emphasis:

- LRU local cache
- Redis second-level cache design
- MySQL metadata persistence
- cache hit/miss metrics
- cache consistency discussion

### Distributed Systems Preparation Version

Emphasis:

- service decomposition
- stateless HTTP search service
- Redis as shared cache
- MySQL as durable metadata store
- future horizontal scaling design

## Daily Schedule

### Day 1: Week 1 Review Lock

Goal:

- Fully lock the blocking server flow.

Tasks:

- Review `createListenSocket`, `serveForever`, `handleClient`, `writeAll`.
- Ensure the current server runs.
- Update README current status.

Output:

- Week 1 flow can be explained in 3 minutes.

### Day 2: epoll Concepts Lock

Goal:

- Fully lock `epoll_fd`, `listen_fd`, `client_fd`, readiness, nonblocking.

Tasks:

- Review `Epoller`.
- Explain `epoll_create1`, `epoll_ctl`, `epoll_wait`.
- Record final Feynman explanation.

Output:

- `docs/functions/Epoller.md` finalized.

### Day 3: EventLoop Integration

Goal:

- Make epoll EventLoop the main server path.

Tasks:

- Review current `serveWithEventLoop`.
- Ensure `listen_fd` and `client_fd` are nonblocking.
- Verify `/` and `/search?q=epoll`.

Output:

- epoll version server works.

### Day 4: Clean Network Module + Project Positioning

Goal:

- Move temporary logic out of `main.cpp` and lock project ecosystem positioning.

Tasks:

- Introduce `TcpServer` or clean helper functions.
- Keep main small.
- Document flow.
- Write how this C++ search service supports later Agent/RAG.

Output:

- Cleaner network structure.
- `docs/project-positioning.md`.

### Day 5: Channel Abstraction

Goal:

- Introduce Reactor-style `Channel`.

Tasks:

- Implement fd + events + callback wrapper.
- Replace raw callback map gradually if feasible.

Output:

- Understand `fd -> Channel -> handleEvent`.

### Day 6: HTTP Query Parsing

Goal:

- Parse `/search?q=xxx` cleanly.

Tasks:

- Implement query extraction.
- Handle missing `q`.
- Return simple JSON response.

Output:

- `/search?q=cpp` returns parsed query.

### Day 7: Week 1-2 Checkpoint

Goal:

- Stabilize epoll HTTP skeleton.

Tasks:

- Build/run.
- Fix compile issues.
- Update README architecture section.

Output:

- GitHub-ready network skeleton.

### Day 8: Document Data Format

Goal:

- Define searchable document format.

Tasks:

- Use simple local text or JSONL documents.
- Add sample documents.
- Decide fields: id, title, content.

Output:

- `data/sample_docs.jsonl` or equivalent.

### Day 9: Document Loader

Goal:

- Load documents into memory.

Tasks:

- Implement `Document`.
- Implement `DocumentLoader`.
- Print loaded count.

Output:

- Server can load sample docs at startup.

### Day 10: Tokenizer

Goal:

- Implement simple English/token tokenizer.

Tasks:

- Lowercase.
- Split by non-alphanumeric chars.
- Remove empty tokens.

Output:

- Tokenizer tests or examples.

### Day 11: Inverted Index

Goal:

- Build term -> doc list index.

Tasks:

- Implement `InvertedIndex::addDocument`.
- Store term frequency.

Output:

- Query term can return candidate docs.

### Day 12: Basic Search

Goal:

- Implement simple keyword search.

Tasks:

- `SearchEngine::search(query)`.
- Return top documents by term hit count.

Output:

- `/search?q=epoll` returns real docs.

### Day 13: Ranking Upgrade

Goal:

- Add TF-IDF or simplified BM25.

Tasks:

- Compute document frequency.
- Score candidates.

Output:

- Ranked results with score.

### Day 14: JSON Response

Goal:

- Return clean JSON response.

Tasks:

- Include id, title, score, snippet.
- Escape basic JSON strings.

Output:

- Search endpoint usable by curl/browser.

### Day 15: Midpoint Stabilization

Goal:

- Make network + search integrated and stable.

Tasks:

- Build/run.
- Fix bugs.
- Update docs.

Output:

- First complete searchable server.

### Day 16: LRU Cache Design + Redis Extension Boundary

Goal:

- Learn and design query cache, and define how Redis can become second-level cache later.

Tasks:

- Understand `unordered_map + list`.
- Define cache key: query string.
- Define cache value: serialized response or result list.
- Define two-level cache flow: local LRU first, Redis second, search engine last.
- Decide that Redis is optional in GitHub demo.

Output:

- LRU design doc.
- Redis extension design note.

### Day 17: Implement LRU Cache

Goal:

- Add local cache.

Tasks:

- `get`.
- `put`.
- capacity eviction.

Output:

- Unit-like examples.

### Day 18: Integrate Cache + Redis Interface Draft

Goal:

- Cache hot query results.

Tasks:

- Check cache before search.
- Fill cache after search.
- Add hit/miss counters.
- Add `Cache` interface or document interface boundary so Redis can be added later.

Output:

- Response/log shows cache hit status.

### Day 19: Basic Metrics + MySQL Boundary

Goal:

- Add simple observability and define MySQL's role.

Tasks:

- Count requests.
- Count cache hits.
- Add `/metrics` endpoint if simple.
- Decide what belongs in MySQL: document metadata, source path, title, updated_at, tags.
- Keep document content/index local for the 30-day demo.

Output:

- Interview hook stronger: measurable behavior.
- MySQL extension note.

### Day 20: Benchmark Script + Cache Comparison

Goal:

- Prepare performance evidence.

Tasks:

- Add curl loop or simple Python benchmark script.
- Compare repeated query with/without cache if feasible.
- Record local LRU value first; Redis comparison is optional.

Output:

- `docs/benchmark.md` first draft.

### Day 21: README First Pass

Goal:

- Make repository understandable.

Tasks:

- Background.
- Features.
- Architecture.
- Build/run.
- Demo commands.

Output:

- GitHub README usable.

### Day 22: Interview Notes + Resume Variants

Goal:

- Prepare project defense.

Tasks:

- Write likely questions.
- Write answers on epoll, EventLoop, index, cache.
- Write three resume variants: C++ backend, search/RAG infrastructure, cache/storage backend.

Output:

- `docs/interview-notes.md`.
- `docs/resume-variants.md`.

### Day 23: Error Handling Cleanup + Redis/MySQL Learning Notes

Goal:

- Improve reliability.

Tasks:

- Check fd close paths.
- Handle missing files.
- Handle empty query.
- Add learning notes: Redis cache penetration/breakdown/avalanche; MySQL index/MVCC basics.

Output:

- Fewer embarrassing edge-case failures.
- `docs/storage-cache-notes.md`.

### Day 24: Code Organization Cleanup + Storage Abstraction

Goal:

- Make code readable to interviewer.

Tasks:

- Move search code into `src/search`.
- Move cache code into `src/cache`.
- Keep `main.cpp` short.
- Keep future extension boundaries clear: `DocumentStore`, `Cache`, `SearchEngine`.

Output:

- Clear module boundaries.

### Day 25: Demo Dataset Polish

Goal:

- Make demo scenario real.

Tasks:

- Add documents about C++ backend, epoll, Redis, projects, job notes.
- Make search examples meaningful.

Output:

- Demo queries look useful.

### Day 26: Benchmark Final + Optional Redis Prototype

Goal:

- Produce final measurable result.

Tasks:

- Run repeated queries.
- Record latency or request count.
- Show cache hit improvement qualitatively or quantitatively.
- If core project is stable, add Redis prototype branch/note only.

Output:

- `docs/benchmark.md` final draft.

### Day 27: GitHub Polish + Distributed Design Note

Goal:

- Make repo presentable.

Tasks:

- Check `.gitignore`.
- Add screenshots or curl examples.
- Add architecture diagram text.
- Add future distributed architecture note: multiple search server instances + Redis shared cache + MySQL metadata.

Output:

- Repo looks intentional.
- `docs/future-distributed-design.md`.

### Day 28: End-to-End Dry Run

Goal:

- Simulate interviewer cloning repo.

Tasks:

- Build from scratch.
- Run server.
- Run demo queries.
- Follow README.

Output:

- README commands verified.

### Day 29: Resume Bullet Draft

Goal:

- Convert project into resume language.

Tasks:

- Write 3-4 bullet points.
- Avoid exaggeration.
- Highlight epoll, search, cache, benchmark.

Output:

- Resume-ready project description.

### Day 30: Final Review

Goal:

- Freeze GitHub version.

Tasks:

- Final code cleanup.
- Final docs.
- Final interview explanation.

Output:

- GitHub-ready C++ backend project.

## Daily Working Rule

At the start of each new day, user will tell Codex which day starts.

For each day:

1. Review yesterday's output briefly.
2. State today's module purpose.
3. Implement only today's scoped tasks.
4. Explain core code.
5. Write local learning notes.
6. End with a short self-check.

## Success Standard

This 30-day version is successful if:

- The project can be built and run.
- The `/search` endpoint solves a real local document search problem.
- The README is clear enough for GitHub.
- The project has one defensible hook: epoll HTTP search service with local index and LRU cache.
- The user can explain the full path from socket to search result.
- The user can explain how Redis/MySQL would extend the project without making the core demo depend on them.
