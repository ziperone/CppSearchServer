# CppSearchServer

CppSearchServer is a Linux C++ backend project for practicing real server-side engineering through a document search service.

The final target is a high-performance HTTP document search server with:

- epoll Reactor networking
- HTTP request parsing
- thread pool based search execution
- inverted index and ranking
- local LRU query cache
- benchmark reports and interview notes

## Current Stage

Week 1: minimal blocking HTTP server.

The current server only proves the most basic lifecycle:

1. Create a listening socket.
2. Bind it to a port.
3. Accept a client connection.
4. Read an HTTP request.
5. Return an HTTP response.

This simple version is intentionally not high-performance yet. It is the baseline that will later be upgraded to epoll Reactor.

## Build On Linux

```bash
cmake -S . -B build
cmake --build build
./build/cpp_search_server 8080
```

Test:

```bash
curl http://127.0.0.1:8080/
curl "http://127.0.0.1:8080/search?q=epoll"
```

## Learning Rule

For each module:

1. Clarify why the module exists.
2. Implement or hand-type the core functions.
3. Run and verify it.
4. Review what was learned.
5. Explain it using the Feynman method.

