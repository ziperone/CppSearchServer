# Week 02.2 - EventLoop Integration

## Module Purpose

This step replaces the Week 1 blocking loop with an epoll-based event loop.

Week 1:

```text
while true -> accept -> handleClient -> close -> repeat
```

Week 2.2:

```text
EventLoop -> epoll_wait -> ready listen_fd/client_fd -> callback
```

## What Changed

`main` now calls:

```cpp
serveWithEventLoop(listen_fd);
```

instead of:

```cpp
serveForever(listen_fd);
```

## Core Flow

```text
create listen_fd
  -> set listen_fd nonblocking
  -> add listen_fd read callback
  -> EventLoop::loop
  -> epoll_wait
  -> listen_fd readable
  -> accept client_fd
  -> set client_fd nonblocking
  -> add client_fd read callback
  -> client_fd readable
  -> handleClient
  -> remove client_fd
  -> close client_fd
```

## Key Code

```cpp
void serveWithEventLoop(int listen_fd) {
    if (net::setNonBlocking(listen_fd) < 0) {
        throw std::runtime_error("set listen fd nonblocking failed");
    }

    net::EventLoop loop;

    loop.addReadEvent(listen_fd, [&loop](int fd) {
        while (true) {
            int client_fd = ::accept(fd, ...);

            if (client_fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                perror("accept");
                break;
            }

            net::setNonBlocking(client_fd);

            loop.addReadEvent(client_fd, [&loop](int ready_fd) {
                handleClient(ready_fd);
                loop.removeEvent(ready_fd);
                net::closeFd(ready_fd);
            });
        }
    });

    loop.loop();
}
```

## Why listen_fd Is Nonblocking

`epoll_wait` tells us `listen_fd` is readable, which means at least one connection can be accepted.

But there may be multiple pending connections.

So the accept callback loops:

```text
accept -> accept -> accept -> EAGAIN -> stop
```

`EAGAIN` means:

```text
No more completed connections are currently waiting.
```

If `listen_fd` were blocking, the final `accept` could block the whole event loop.

## Why client_fd Is Nonblocking

`client_fd` is also registered into epoll.

When it becomes readable:

```text
client has sent request bytes
```

This version still reuses `handleClient`, then closes the connection.

This is not the final production design. Later, `client_fd` will need:

- input buffer
- output buffer
- HTTP parser
- write event handling
- keep-alive support

## What EventLoop Adds

Before:

```text
serveForever directly decides when to call accept and handleClient.
```

Now:

```text
EventLoop waits for fd readiness and dispatches callbacks.
```

In this version:

```text
listen_fd callback -> accept new clients
client_fd callback -> handle one request
```

## What This Improves

This version no longer blocks waiting for one not-ready fd.

The server can manage many connections in one event loop:

```text
not-ready fd -> ignored until epoll reports it
ready fd -> callback runs
```

## Current Limitations

This is still an intermediate version:

- `handleClient` still reads only once.
- `writeAll` is still blocking-style.
- client connection is closed after one request.
- no write event callback yet.
- no `Channel` abstraction yet.
- no thread pool yet.

The next design step is `Channel`.

## Feynman Explanation

In Week 1, the server stood at the door and served one client completely before seeing the next one.

Now, `EventLoop` asks epoll which fd is ready. If `listen_fd` is ready, it accepts all currently waiting clients and registers each `client_fd`. If a `client_fd` is ready, it means that client sent data, so the loop calls `handleClient` for that fd.

The event loop is still single-threaded, but it avoids waiting on connections that are not ready.

## Review Questions

1. Why must `listen_fd` be nonblocking?
2. Why does the accept callback use `while (true)`?
3. What does `EAGAIN` mean in the accept loop?
4. Why do we register `client_fd` after accepting it?
5. What does the `client_fd` callback do in this version?
6. Why is this still not the final high-performance design?

## Learned Summary

After learning this step, remember only this:

> `EventLoop` replaces the blocking accept loop: epoll reports ready fds, the listen callback accepts new clients, and client callbacks handle readable connections.

