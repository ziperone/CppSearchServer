# Week 01 Review - Minimal Server

## Module Purpose

This module builds the smallest useful HTTP server lifecycle:

```text
socket -> bind -> listen -> accept -> recv -> route -> send -> close
```

It is intentionally simple. The purpose is to understand the foundation before adding epoll, Reactor, thread pool, and search logic.

## What Has Been Implemented

- Project skeleton with CMake.
- `net::createListenSocket`.
- `net::setNonBlocking`.
- `net::closeFd`.
- Minimal HTTP response builder.
- Blocking `serveForever` loop.
- Basic route handling:
  - `/`
  - `/search?q=...`

## What You Should Learn

### 1. Listening fd and connected fd are different

The listening fd is only responsible for accepting new connections.

`accept` returns a new connected fd. That connected fd represents one client connection and is used for `recv` and `send`.

### 2. HTTP is only bytes on top of TCP

TCP only provides a byte stream. The server receives bytes and interprets them as an HTTP request.

In this first version, we only parse the request line enough to extract the path.

### 3. Blocking IO is easy but not scalable

This version handles one client at a time. If one client is slow, the server can be blocked.

This is exactly why Week 2 will introduce epoll Reactor.

### 4. System calls need error handling

Almost every system call can fail:

- `socket`
- `setsockopt`
- `bind`
- `listen`
- `accept`
- `recv`
- `send`

A backend engineer must develop the habit of checking return values.

## Feynman Explanation

If I explain this module to a beginner:

"A C++ server asks Linux for a TCP socket, binds it to a port, and starts listening. The listening socket is like the server entrance. When a client connects, `accept` creates a new socket just for that client. Then the server reads bytes from that client, figures out which path was requested, builds an HTTP response string, sends it back, and closes the connection. This version works, but it handles clients one by one, so it is only the baseline. To support many connections, we need epoll next."

## Current Verification Status

The project has been compiled and verified on the remote Linux server.

Remote environment:

- Host: `10.195.112.130`
- User: `a704`
- Project path: `/home/a704/Desktop/sseattack/CppSearchServer`
- OS: Ubuntu 24.04.3 LTS
- Compiler: g++ 13.3.0
- CMake: 3.28.3

Build commands:

```bash
cd /home/a704/Desktop/sseattack/CppSearchServer
cmake -S . -B build
cmake --build build
```

Run command:

```bash
./build/cpp_search_server 8080
```

Verified requests:

```bash
curl http://127.0.0.1:8080/
curl "http://127.0.0.1:8080/search?q=epoll"
```

Verified responses:

```text
Hello CppSearchServer
```

```json
{"message":"search endpoint is reserved for week 5","query":"q=epoll"}
```

Runtime status:

- The service listened on `0.0.0.0:8080`.
- Verified process PID during test: `69693`.

Historical note:

Local Windows compilation was not used because the local environment had no direct `g++`, `cmake`, or configured WSL Linux distribution.

When a Linux/WSL environment is available, verify with:

```bash
cd /mnt/d/FindJob/CppSearchServer
cmake -S . -B build
cmake --build build
./build/cpp_search_server 8080
curl http://127.0.0.1:8080/
curl "http://127.0.0.1:8080/search?q=epoll"
```

## Before Moving To Week 2

You should be able to answer:

1. Why does `accept` return a new fd?
2. Why do we need `SO_REUSEADDR`?
3. What does `Content-Length` do in an HTTP response?
4. Why can this server only handle one client at a time?
5. What problem will epoll solve?
