# Week 02 - epoll Reactor Plan

## Why Week 2 Exists

Week 1 has a working HTTP server, but it is blocking and serial:

```text
accept -> handle one client -> close -> accept next
```

The problem:

> A slow client can block the whole server.

Week 2 upgrades the server from blocking serial processing to event-driven connection management.

## Week 2 Goal

Build the first epoll-based Reactor skeleton:

```text
listen_fd
  -> epoll
  -> EventLoop
  -> accept readable events
  -> client readable events
  -> handle request
```

At the end of Week 2, the server should still return the same responses as Week 1, but the architecture should be ready for many connections.

## Current Starting Point

2026-07-09:

- Week 1 blocking flow has been reviewed.
- The known bottleneck is the serial path: `accept -> handleClient -> close`.
- More precisely, `accept`, `recv`, and `send` can all block in the current model.
- Week 2 starts with epoll readiness learning before writing the `Epoller` wrapper.

## Modules To Add

### Epoller

Responsibility:

```text
Wrap epoll_create1, epoll_ctl, epoll_wait.
```

Core functions to hand-type:

- `Epoller::Epoller()`
- `Epoller::addFd`
- `Epoller::modFd`
- `Epoller::delFd`
- `Epoller::wait`

### Channel

Responsibility:

```text
Represent one fd and the events/callbacks attached to it.
```

Core ideas:

- fd
- interested events
- returned events
- read callback
- write callback

Core function to hand-type:

- `Channel::handleEvent`

### EventLoop

Responsibility:

```text
Own epoll and run the event dispatch loop.
```

Core function to hand-type:

- `EventLoop::loop`

Status:

- Created `src/net/EventLoop.h`.
- Created `src/net/EventLoop.cpp`.
- Created `docs/functions/EventLoop.md`.
- Current version supports read callbacks only.

### TcpServer Basic Version

Responsibility:

```text
Create listen_fd, register it into EventLoop, accept new clients when listen_fd becomes readable.
```

Core function to understand:

- `handleAccept`

## Implementation Order

### Step 1: Make listen_fd nonblocking

Why:

```text
epoll servers should avoid blocking on accept/recv/send.
```

Key function:

- `setNonBlocking`

### Step 2: Add Epoller

Why:

```text
Let the kernel tell us which fds are ready instead of blocking on one fd.
```

Status:

- Created `src/net/Epoller.h`.
- Created `src/net/Epoller.cpp`.
- Created `docs/functions/Epoller.md`.
- `Epoller` currently wraps only epoll operations and is not yet integrated into `main`.
- Remote build passed on Ubuntu server after adding `Epoller`.

### Step 3: Register listen_fd

Why:

```text
When listen_fd is readable, it means new connections are ready to accept.
```

### Step 4: Accept clients in a loop

Why:

```text
In edge-triggered mode later, one event may represent multiple pending connections.
```

For first version, LT mode is acceptable.

### Step 5: Register client_fd readable events

Why:

```text
client_fd readable means the client has sent request bytes.
```

### Step 6: Reuse handleClient temporarily

Why:

```text
Keep HTTP logic unchanged while replacing the network event model.
```

Later, `handleClient` will be split into connection buffer and HTTP parser.

## Week 2 Learning Focus

You must understand:

- Why blocking accept/recv is a problem.
- What readiness notification means.
- Why `listen_fd` readable means "can accept".
- Why `client_fd` readable means "can recv".
- What epoll stores.
- What epoll returns.
- Why fd should be nonblocking.
- Difference between LT and ET at a high level.

## Week 2 Pass Standard

Week 2 is complete when:

- The server uses epoll for `listen_fd`.
- Client connections are accepted from epoll events.
- Client read events can be handled.
- `/` and `/search?q=epoll` still work.
- You can explain why this is better than Week 1 blocking loop.

## Current Local Implementation Status

2026-07-14:

- `main.cpp` now uses `serveWithEventLoop(listen_fd)`.
- `listen_fd` is set to nonblocking mode before being registered.
- `listen_fd` read callback accepts pending clients.
- each accepted `client_fd` is set to nonblocking mode and registered into `EventLoop`.
- `client_fd` read callback temporarily reuses `handleClient`, then removes and closes the fd.
- `serveForever` is kept only as a historical Week 1 reference.
- Remote deployment/verification is optional; local code and docs are the primary learning record.
