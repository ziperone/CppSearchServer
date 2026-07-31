# EventLoop

## Module Purpose

`EventLoop` is the dispatcher above `Epoller`.

In one sentence:

> `EventLoop` waits for ready fd events from epoll and calls the callback registered for each ready fd.

`Epoller` answers:

```text
Which fds are ready?
```

`EventLoop` answers:

```text
Who should handle this ready fd?
```

## Source Location

- `src/net/EventLoop.h`
- `src/net/EventLoop.cpp`

## Current Design

This is a minimal Week 2 version.

It supports:

- registering readable fd callbacks
- removing fd events
- waiting in a loop
- dispatching `EPOLLIN` events to callbacks

It does not yet support:

- write callbacks
- Channel abstraction
- timers
- cross-thread wakeup
- connection buffers

## Core Flow

```text
addReadEvent(fd, callback)
  -> epoller.addFd(fd, EPOLLIN)

loop()
  -> epoller.wait()
  -> for each ready event
  -> dispatch(fd, events)
  -> callback(fd)
```

## Code Skeleton

```cpp
class EventLoop {
public:
    using EventCallback = std::function<void(int)>;

    void addReadEvent(int fd, EventCallback callback);
    void removeEvent(int fd);

    void loop();
    void quit();

private:
    void dispatch(int fd, std::uint32_t events);

    bool quit_;
    Epoller epoller_;
    std::unordered_map<int, EventCallback> read_callbacks_;
};
```

## Key Concepts

### callback

A callback is a function saved for later execution.

Example:

```cpp
loop.addReadEvent(listen_fd, [](int fd) {
    // accept new clients
});
```

Meaning:

```text
When listen_fd becomes readable, call this function.
```

### read_callbacks_

```cpp
std::unordered_map<int, EventCallback> read_callbacks_;
```

This maps:

```text
fd -> function that handles fd readable event
```

Example:

```text
listen_fd -> handleAccept
client_fd -> handleClient
```

### addReadEvent

```cpp
void EventLoop::addReadEvent(int fd, EventCallback callback) {
    read_callbacks_[fd] = std::move(callback);
    epoller_.addFd(fd, EPOLLIN);
}
```

Two things happen:

1. Save the callback in user space.
2. Register the fd into kernel epoll.

### loop

```cpp
void EventLoop::loop() {
    while (!quit_) {
        auto events = epoller_.wait();
        for (const auto& event : events) {
            dispatch(event.data.fd, event.events);
        }
    }
}
```

This is the event loop:

```text
wait -> get ready events -> dispatch -> repeat
```

### dispatch

```cpp
void EventLoop::dispatch(int fd, std::uint32_t events)
```

`dispatch` decides what to do with a ready event.

In the current version:

- `EPOLLERR` / `EPOLLHUP`: remove fd
- `EPOLLIN`: find read callback and call it

## Relationship With Epoller

`Epoller` only wraps Linux system calls.

`EventLoop` owns `Epoller` and adds application-level dispatch.

```text
Epoller:
  epoll_create1 / epoll_ctl / epoll_wait

EventLoop:
  while loop / callback table / event dispatch
```

## Relationship With Reactor

This is the beginning of Reactor:

```text
fd readiness event -> dispatch to handler
```

Later, `Channel` will replace the raw callback map:

```text
fd -> Channel -> handleEvent -> read/write callback
```

## Feynman Explanation

`Epoller` is the notifier. It tells us which fd is ready.

`EventLoop` is the dispatcher. It keeps a table saying which function should handle each fd. The loop sleeps in `epoll_wait`. When epoll returns ready events, EventLoop checks the fd, finds the registered callback, and calls it.

So if `listen_fd` is ready, EventLoop calls the accept callback. If `client_fd` is ready, EventLoop calls the client read callback.

## Review Questions

1. What problem does `EventLoop` solve above `Epoller`?
2. What is a callback?
3. Why do we need `read_callbacks_`?
4. What does `loop()` repeatedly do?
5. What is the difference between `epoller_.wait()` and `dispatch()`?
6. Why is this already the beginning of Reactor?

## Learned Summary

After learning this module, remember only this:

> `EventLoop` repeatedly waits for epoll ready events and dispatches each ready fd to its registered callback.

