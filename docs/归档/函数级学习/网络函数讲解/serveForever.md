# serveForever

## Function Purpose

`serveForever` is the main blocking server loop in Week 1.

In one sentence:

> It repeatedly waits for client connections on `listen_fd`, accepts one connection, handles it, closes the connected fd, and then waits for the next one.

## Source Location

- `src/main.cpp`

## Core Flow

```text
while true -> accept -> handleClient -> close client_fd -> repeat
```

## Code

```cpp
void serveForever(int listen_fd) {
    while (true) {
        sockaddr_storage client_addr {};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        handleClient(client_fd);
        net::closeFd(client_fd);
    }
}
```

## Key Points

### while true

The server should keep running instead of handling only one client.

In this first version, `while (true)` means:

```text
as long as the process is alive, keep accepting clients
```

This is why the function is named `serveForever`.

### sockaddr_storage

`sockaddr_storage` is a generic address container.

It is large enough to store different client address types, including IPv4 and IPv6.

In this version, we do not use the client address yet, but `accept` requires these parameters if we want to receive client address information.

### socklen_t

`client_len` tells `accept` the size of the address buffer.

After `accept` returns, it may be updated to the actual client address length.

### accept

`accept(listen_fd, ...)` takes one completed connection from the kernel's pending connection queue.

If successful, it returns a new connected fd:

```text
listen_fd: server entrance, used for accepting connections
client_fd: one specific TCP connection, used for recv/send
```

In blocking mode, if there is no connection waiting, `accept` blocks and the program sleeps there.

### error handling

If `accept` fails, the code prints the error and continues the loop.

This means one failed accept should not kill the entire server.

### handleClient

`handleClient(client_fd)` handles the connected client:

```text
recv -> parse path -> route -> build response -> send
```

This function is blocking in Week 1. While it handles one client, the server cannot accept another one.

### close client_fd

After handling the request, the server closes `client_fd`.

This version uses short connections:

```text
one request -> one response -> close connection
```

The listening fd remains open. Only the connected fd is closed.

## Important Distinction

Do not confuse these two:

```text
listen_fd: long-lived server entrance
client_fd: short-lived client connection
```

`serveForever` never closes `listen_fd`.

It only closes each `client_fd` after handling that client.

## Blocking Model Limitation

This Week 1 version is simple but weak:

```text
accept one client -> handle this client completely -> accept next client
```

If one client is slow, the whole server is delayed.

This is the reason we will introduce epoll Reactor later.

## Hand-Type Version

Type this version from memory:

```cpp
void serveForever(int listen_fd) {
    while (true) {
        sockaddr_storage client_addr {};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = ::accept(
            listen_fd,
            reinterpret_cast<sockaddr*>(&client_addr),
            &client_len
        );

        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        handleClient(client_fd);
        net::closeFd(client_fd);
    }
}
```

## Feynman Explanation

`serveForever` is the server's front desk loop.

`listen_fd` is the entrance. The loop waits at `accept`. When a client connection arrives, `accept` gives us a new `client_fd`. That new fd represents this specific client. The server passes it to `handleClient`, sends a response, closes the client fd, and goes back to `accept` to wait for the next client.

## Review Questions

1. Why is there a `while (true)`?
2. What does `accept` return?
3. Why does the function close `client_fd` but not `listen_fd`?
4. What happens in blocking mode if no client connects?
5. Why is this model bad for high concurrency?
6. What is the relationship between `serveForever` and the later epoll Reactor?

## Learned Summary

After learning this function, remember only this:

> `serveForever` is a blocking accept loop: wait on `listen_fd`, get one `client_fd`, handle it, close it, and repeat forever.

## 2026-07-08 Learning Check

User's current understanding:

- Correctly understood that `while (true)` keeps the server continuously accepting client connections.
- Correctly understood that `listen_fd` belongs to the server listening entrance, while `client_fd` belongs to a specific client connection.
- Correctly understood that there can be many `client_fd`s over time, but this Week 1 server has one `listen_fd` for one listening port.
- Correctly understood that `client_fd` should be closed after `handleClient` finishes, otherwise resources will be occupied.
- Correctly understood that `listen_fd` should remain open because the server still needs to accept later connections.

Important missing point:

- This model is bad for high concurrency because `accept -> handleClient -> close` is executed serially in one thread. While `handleClient` is blocked on a slow client's `recv` or `send`, the server cannot accept and process other clients.

Final Chinese summary:

> `serveForever` 是第一周的阻塞式主循环。它一直调用 `accept` 等待连接；每次连接到来，`accept` 基于 `listen_fd` 生成一个 `client_fd`；服务器用 `client_fd` 调用 `handleClient` 完成一次请求处理，然后关闭 `client_fd`，再回到 `accept` 等下一个连接。它的问题是串行阻塞，慢连接会拖住整个服务器。
