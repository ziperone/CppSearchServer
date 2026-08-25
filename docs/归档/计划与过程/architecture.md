# Architecture

## Stage 1 Architecture

```text
client
  |
  | HTTP request
  v
listening socket
  |
  | accept
  v
connected socket
  |
  | read request bytes
  v
request router
  |
  | build response
  v
connected socket
  |
  | write response bytes
  v
client
```

## Planned Architecture

```text
client
  |
  v
epoll Reactor
  |
  v
HTTP parser
  |
  v
SearchService
  |
  +-- LRU cache
  |
  +-- SearchEngine
        |
        +-- InvertedIndex
        +-- Ranker
```

