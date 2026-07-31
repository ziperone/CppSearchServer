# Week 02.1 - epoll Basics

## Module Purpose

Week 1 server works, but it is blocking and serial:

```text
accept -> handleClient -> recv -> route -> send -> close -> next client
```

The core problem:

> The server waits on one operation at a time. A slow client can block all later clients.

`epoll` is introduced to solve the waiting problem:

> Instead of blocking on one fd, ask the kernel which fds are ready.

## What Is Readiness

Readiness does not mean "the work is finished".

Readiness means:

```text
This fd can probably perform an operation now without blocking.
```

Examples:

- `listen_fd` readable: there are completed TCP connections waiting for `accept`.
- `client_fd` readable: there are request bytes waiting for `recv`.
- `client_fd` writable: the socket send buffer has space for `send`.

## Week 1 vs Week 2

### Week 1 Blocking Model

```text
while true:
    client_fd = accept(listen_fd)
    handleClient(client_fd)
    close(client_fd)
```

Problem:

```text
If accept/recv/send blocks, the whole process waits.
```

### Week 2 epoll Model

```text
register listen_fd into epoll

while true:
    ready_fds = epoll_wait()
    for each ready fd:
        if fd is listen_fd:
            accept new clients
        else:
            recv/send client data
```

Key change:

```text
The server no longer asks one fd "are you ready?"
The server asks the kernel "which fds are ready?"
```

## Why fd Must Be Nonblocking

`epoll` tells us an fd is ready, but readiness is not a permanent guarantee.

If we still use blocking fd, one operation may still block unexpectedly.

So epoll servers usually set sockets to nonblocking mode:

```text
ready -> try operation -> if no more data, return EAGAIN instead of blocking
```

This is especially important for edge-triggered mode later.

## What epoll Stores

You can think of epoll as a kernel-side interest list:

```text
fd A: I care about readable events
fd B: I care about readable events
fd C: I care about writable events
```

Important:

> epoll does not store request data. It stores fd event interests and returns ready events.

## Three Core System Calls

### epoll_create1

Creates an epoll instance.

```text
epoll_fd = epoll_create1(0)
```

`epoll_fd` itself is also a file descriptor.

### epoll_ctl

Adds, modifies, or deletes fd interests.

Common operations:

- `EPOLL_CTL_ADD`
- `EPOLL_CTL_MOD`
- `EPOLL_CTL_DEL`

### epoll_wait

Waits for ready events.

```text
number_of_ready_events = epoll_wait(epoll_fd, events, max_events, timeout)
```

If no fd is ready, `epoll_wait` can block.

But this block is different from Week 1:

```text
Week 1 blocks on one fd.
epoll_wait blocks waiting for any registered fd to become ready.
```

## First Week 2 Target

The first code target is not full Reactor yet.

Target:

```text
Use epoll to monitor listen_fd.
When listen_fd becomes readable, accept client connections.
```

Then extend:

```text
Register client_fd into epoll.
When client_fd becomes readable, call handleClient temporarily.
```

## Feynman Explanation

In Week 1, the server stands in front of one door and waits. If one visitor talks slowly, everyone behind waits.

With epoll, the server gives the kernel a list of doors and says: "Tell me which doors have people ready." The server sleeps in `epoll_wait`. When the kernel reports ready fds, the server handles those fds one by one.

`listen_fd` ready means there are new connections to accept. `client_fd` ready means there is data to read or space to write.

## Review Questions

1. What does "fd readable" mean for `listen_fd`?
2. What does "fd readable" mean for `client_fd`?
3. Does epoll store HTTP request data?
4. Why should epoll-based sockets be nonblocking?
5. What is the difference between blocking on `accept` and blocking on `epoll_wait`?
6. Why does epoll naturally solve the Week 1 slow-client problem?

## Learned Summary

After learning this module, remember only this:

> epoll lets the server wait for readiness across many fds. `listen_fd` readiness means accept connections; `client_fd` readiness means read/write client data. epoll stores event interests, not request data.

## 2026-07-09 Learning Check

User's current understanding:

- Correctly understood that epoll changes the notification/scheduling model, not the actual HTTP data processing logic.
- Correctly understood that epoll does not store request data.
- Correctly understood that if fds are still blocking, the server can still be stuck even with many fds registered in epoll.

Corrections:

- `listen_fd` readable does not simply mean "`listen_fd` is usable". It specifically means there are completed TCP connections waiting in the kernel accept queue, so `accept` can be called without blocking.
- `client_fd` readable does not mean "later client fds can be scheduled without waiting for previous client fd to finish". It specifically means this `client_fd` has bytes available in its socket receive buffer, so `recv` can be attempted without blocking.
- epoll does not automatically make requests execute in parallel. In a single-threaded event loop, ready fds are still handled one by one, but the loop avoids blocking on fds that are not ready.

Final Chinese summary:

> `listen_fd` 可读表示有已完成握手的连接可以 `accept`；`client_fd` 可读表示这个连接的接收缓冲区里有数据可以 `recv`。epoll 只告诉我们哪些 fd 就绪，不保存请求数据，也不自动并行处理业务。fd 仍要设置为非阻塞，是为了避免某个就绪 fd 在实际 `accept/recv/send` 时再次把整个事件循环卡住。

## 2026-07-14 Single-Thread epoll Review

User can now explain:

- The blocking server creates `listen_fd`, binds it to a port, listens, accepts connections, receives request bytes, and sends responses through `client_fd`.
- `epoll_fd` represents a kernel epoll instance.
- epoll watches both `listen_fd` and `client_fd` readiness.
- When `listen_fd` is ready, the server can call `accept`.
- When `client_fd` is ready, the server can call `recv`.
- Single-thread epoll improves concurrency by avoiding blocking on one not-ready fd, but it does not create CPU parallelism.

Corrections to remember:

- `listen_fd` does not represent "connection data". It represents the listening socket. Completed TCP connections wait in the kernel accept queue behind it.
- `accept` does not "accept connection data". It takes one completed TCP connection from the accept queue and returns a local `client_fd`.
- `client_fd` does not represent data itself. It represents one specific TCP connection; request bytes live in kernel socket buffers.
- epoll is not application-level polling over every fd. The program blocks in `epoll_wait`; the kernel returns only ready events.
- epoll does not "check client_fd after listen_fd" as a fixed sequence. Each `epoll_wait` may return `listen_fd`, one or more `client_fd`s, or both, depending on which fds are ready.

Final Chinese summary:

> 单线程 epoll 的本质是：把 `listen_fd` 和多个 `client_fd` 注册到内核 epoll 实例中，事件循环阻塞在 `epoll_wait`，由内核返回就绪事件。`listen_fd` 可读表示有连接可 `accept`；`client_fd` 可读表示该连接有数据可 `recv`。事件循环仍然逐个处理就绪事件，但不会卡在未就绪 fd 上，因此能用一个线程管理大量连接。
