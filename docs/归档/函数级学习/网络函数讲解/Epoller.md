# Epoller

## Module Purpose

`Epoller` is a small C++ RAII wrapper around Linux epoll.

In one sentence:

> `Epoller` owns an epoll fd, lets us add/modify/delete watched fds, and waits for ready events.

This module does not parse HTTP, does not accept clients by itself, and does not store request data.

## Source Location

- `src/net/Epoller.h`
- `src/net/Epoller.cpp`

## Core Flow

```text
epoll_create1 -> epoll_ctl ADD/MOD/DEL -> epoll_wait -> return ready events
```

## Public Interface

```cpp
class Epoller {
public:
    explicit Epoller(int max_events = 1024);
    ~Epoller();

    void addFd(int fd, std::uint32_t events);
    void modFd(int fd, std::uint32_t events);
    void delFd(int fd);

    std::vector<epoll_event> wait(int timeout_ms = -1);
};
```

## Key Concepts

### epoll fd

```cpp
epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
```

`epoll_create1` creates an epoll instance in the kernel.

The returned `epoll_fd_` is also a file descriptor. It represents the epoll object itself.

Do not confuse:

```text
listen_fd: server listening socket
client_fd: one client connection
epoll_fd: kernel epoll instance
```

### events_

```cpp
std::vector<epoll_event> events_;
```

This is user-space storage used by `epoll_wait`.

The kernel writes ready events into this array when `epoll_wait` returns.

### addFd

```cpp
void Epoller::addFd(int fd, std::uint32_t events) {
    update(EPOLL_CTL_ADD, fd, events);
}
```

Meaning:

```text
I want epoll to watch this fd for these events.
```

Example:

```cpp
epoller.addFd(listen_fd, EPOLLIN);
```

This means:

```text
Tell me when listen_fd becomes readable.
```

### modFd

```cpp
void Epoller::modFd(int fd, std::uint32_t events) {
    update(EPOLL_CTL_MOD, fd, events);
}
```

Meaning:

```text
Change what events I care about for this fd.
```

Later example:

```text
After response data is ready, also care about writable events.
```

### delFd

```cpp
void Epoller::delFd(int fd) {
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
}
```

Meaning:

```text
Stop watching this fd.
```

Usually used when closing a client connection.

### wait

```cpp
std::vector<epoll_event> Epoller::wait(int timeout_ms) {
    int n = ::epoll_wait(epoll_fd_, events_.data(), events_.size(), timeout_ms);
    ...
}
```

Meaning:

```text
Block until one or more watched fds become ready, then return those ready events.
```

If `timeout_ms = -1`, wait forever.

If `epoll_wait` is interrupted by a signal and returns `EINTR`, this wrapper returns an empty list.

## update

```cpp
void Epoller::update(int operation, int fd, std::uint32_t events) {
    epoll_event event {};
    event.events = events;
    event.data.fd = fd;

    epoll_ctl(epoll_fd_, operation, fd, &event);
}
```

`event.events` means:

```text
What events do I care about?
```

`event.data.fd` means:

```text
When this event is ready, give me this fd back.
```

## Important Distinction

epoll stores event interests, not HTTP data.

```text
epoll knows: fd 8 is readable.
epoll does not know: GET /search?q=epoll HTTP/1.1.
```

Actual request bytes are still read later by `recv(client_fd, ...)`.

## Hand-Type Functions

You should manually type these:

```cpp
Epoller::Epoller(int max_events)
```

```cpp
void Epoller::addFd(int fd, std::uint32_t events)
```

```cpp
void Epoller::modFd(int fd, std::uint32_t events)
```

```cpp
void Epoller::delFd(int fd)
```

```cpp
std::vector<epoll_event> Epoller::wait(int timeout_ms)
```

```cpp
void Epoller::update(int operation, int fd, std::uint32_t events)
```

## Feynman Explanation

`Epoller` is like a registration desk for file descriptors.

We create an epoll instance with `epoll_create1`. Then we tell it which fds we care about using `epoll_ctl`. For example, we can register `listen_fd` and say "tell me when it is readable." Then the server calls `epoll_wait`, sleeps there, and the kernel wakes it up when some registered fd is ready.

`Epoller` itself does not process requests. It only reports which fds are ready.

## Review Questions

1. What is `epoll_fd_`?
2. What is the difference between `epoll_fd_`, `listen_fd`, and `client_fd`?
3. What does `addFd(fd, EPOLLIN)` mean?
4. What does `event.data.fd = fd` do?
5. What does `epoll_wait` return?
6. Does epoll store HTTP request bytes?
7. Why does `delFd` matter before closing a connection?

## Learned Summary

After learning this module, remember only this:

> `Epoller` wraps the epoll interest list: add fd interests, modify them, delete them, and wait for the kernel to return ready fd events.

## 2026-07-14 Learning Check

User's current understanding:

- Correctly understood that `epoll_fd` is like a listener/monitor for fd readiness.
- Correctly understood that both `listen_fd` and `client_fd` can be registered into the same epoll instance.
- Correctly understood that epoll helps avoid blocking on one not-ready fd.
- Correctly understood the rough flow: register `listen_fd`, wait for readiness, accept connections, register `client_fd`, then wait for client data readiness.

Corrections:

- epoll does not "ensure the server listens normally". `listen` makes the socket listen; epoll only reports readiness events.
- `listen_fd` readiness means there are completed connections waiting for `accept`, not that the listening state itself is healthy.
- The code that receives epoll notifications and decides what function to call is the event loop. In the next step, this role will be implemented by `EventLoop`.

Final Chinese summary:

> `epoll_fd` 是内核 epoll 实例的句柄，像一个 fd 就绪监听器。`addFd(listen_fd, EPOLLIN)` 表示关注新连接是否可 `accept`；`addFd(client_fd, EPOLLIN)` 表示关注这个连接是否有数据可 `recv`。`epoll_wait` 返回就绪事件后，本身不会处理业务，真正分发处理的是事件循环：如果返回的是 `listen_fd`，就调用 accept；如果返回的是 `client_fd`，就调用读请求/处理响应逻辑。
