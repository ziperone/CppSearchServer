# createListenSocket

## Function Purpose

`createListenSocket` creates a TCP listening socket for the server.

In one sentence:

> It asks Linux for a TCP socket, binds it to a port, marks it as a listening socket, and returns the listening file descriptor.

## Source Location

- `src/net/Socket.h`
- `src/net/Socket.cpp`

## Core Flow

```text
socket -> setsockopt -> fill sockaddr_in -> bind -> listen -> return listen_fd
```

## Code

```cpp
int createListenSocket(std::uint16_t port, int backlog) {
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        throwSystemError("socket failed");
    }

    int reuse = 1;
    if (::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        closeFd(listen_fd);
        throwSystemError("setsockopt SO_REUSEADDR failed");
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        closeFd(listen_fd);
        throwSystemError("bind failed");
    }

    if (::listen(listen_fd, backlog) < 0) {
        closeFd(listen_fd);
        throwSystemError("listen failed");
    }

    return listen_fd;
}
```

## Key Points

### socket

`socket(AF_INET, SOCK_STREAM, 0)` creates an IPv4 TCP socket.

- `AF_INET`: IPv4.
- `SOCK_STREAM`: reliable byte stream, which means TCP.
- `0`: let the system choose the default protocol for this family and type.

The return value is a file descriptor. If it is negative, creation failed.

### SO_REUSEADDR

`SO_REUSEADDR` allows the server to bind the same port again after a quick restart.

Without it, the port may still be affected by previous TCP states, and `bind` may fail with `Address already in use`.

### sockaddr_in

`sockaddr_in` describes the local address the server wants to bind.

- `sin_family = AF_INET`: IPv4 address family.
- `sin_addr.s_addr = htonl(INADDR_ANY)`: listen on all local network interfaces.
- `sin_port = htons(port)`: convert port from host byte order to network byte order.

### bind

`bind` attaches the socket to the local IP and port.

After `bind`, the OS knows which port this server wants to receive traffic from.

### listen

`listen` turns the socket into a passive listening socket.

After this step, the fd is not used to exchange HTTP data directly. It is used by `accept` to create connected sockets.

### error handling

If `setsockopt`, `bind`, or `listen` fails after the socket has been created, the function must close `listen_fd` before throwing.

This prevents file descriptor leaks.

## Hand-Type Version

Type this version from memory:

```cpp
int createListenSocket(std::uint16_t port, int backlog) {
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        throwSystemError("socket failed");
    }

    int reuse = 1;
    if (::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        closeFd(listen_fd);
        throwSystemError("setsockopt failed");
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        closeFd(listen_fd);
        throwSystemError("bind failed");
    }

    if (::listen(listen_fd, backlog) < 0) {
        closeFd(listen_fd);
        throwSystemError("listen failed");
    }

    return listen_fd;
}
```

## Feynman Explanation

`createListenSocket` is like opening the front door of a server.

First, `socket` asks Linux to create a TCP socket. Then `setsockopt` makes quick restart easier. Next, we fill an address structure saying "listen on all local IP addresses and this port". `bind` attaches the socket to that address. `listen` changes it from an ordinary socket into a listening socket. Finally, we return the listening fd so the main loop can call `accept` on it.

## Review Questions

1. Why does this function return `listen_fd` instead of a connected client fd?
2. What does `SOCK_STREAM` mean?
3. Why do we need `htons(port)`?
4. What does `INADDR_ANY` mean?
5. What problem does `SO_REUSEADDR` reduce?
6. Why must we call `closeFd(listen_fd)` before throwing after `socket` succeeds?
7. What is the difference between `bind` and `listen`?

## Learned Summary

After learning this function, remember only this:

> `createListenSocket` completes the server-side TCP listening setup: create TCP fd, enable address reuse, bind IP/port, enter listen state, and return the listening fd for `accept`.

## 2026-07-08 Learning Check

User's current understanding:

- Correctly understood the main flow: `socket -> sockaddr_in -> bind -> listen -> return listen_fd`.
- Correctly understood that `listen_fd` itself is not used for HTTP data transfer. It is used to accept connections.
- Correctly understood that `bind` connects the fd with a local IP/port.
- Correctly understood that `listen` starts listening.

Corrections:

- `listen_fd` is a file descriptor. It can later be registered into epoll as an event source, but in this Week 1 blocking version it is not yet an "event descriptor".
- `backlog` is not a lifecycle timeout and does not decide whether an event is expired. It controls the size of the pending connection queue maintained by the kernel.
- `htons(port)` converts the port from host byte order to network byte order. Network protocols use big-endian byte order.
- `accept` returns a new connected fd because the listening fd represents the server entrance, while each connected fd represents one specific TCP connection.

Final Chinese summary:

> `createListenSocket` 的作用是创建服务器入口。`socket` 创建 TCP fd，`setsockopt` 允许端口快速复用，`sockaddr_in` 描述监听地址和端口，`bind` 把 fd 和地址绑定，`listen` 让 fd 进入监听状态，最后返回 `listen_fd`，后续由 `accept` 基于它生成真正用于收发数据的连接 fd。
