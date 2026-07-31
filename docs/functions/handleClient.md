# handleClient

## Function Purpose

`handleClient` handles one connected client in the Week 1 blocking server.

In one sentence:

> It reads HTTP request bytes from `client_fd`, extracts the requested path, routes it to a response, and writes the response back to the same client.

## Source Location

- `src/main.cpp`

## Core Flow

```text
recv -> build request string -> extractPath -> route -> writeAll
```

## Code

```cpp
void handleClient(int client_fd) {
    std::array<char, 4096> buffer {};
    ssize_t n = ::recv(client_fd, buffer.data(), buffer.size() - 1, 0);
    if (n <= 0) {
        return;
    }

    std::string request(buffer.data(), static_cast<std::size_t>(n));
    std::string path = extractPath(request);
    std::string response = route(path);
    writeAll(client_fd, response);
}
```

## Key Points

### client_fd

`client_fd` represents one specific TCP connection.

It is different from `listen_fd`:

```text
listen_fd: accepts new connections
client_fd: communicates with one client
```

`handleClient` only works with `client_fd`.

### buffer

```cpp
std::array<char, 4096> buffer {};
```

This is a fixed-size receive buffer.

In this simple version, we assume one HTTP request fits into 4096 bytes.

This is not robust enough for production. Later, the HTTP parser will need to support partial reads and accumulated buffers.

### recv

```cpp
ssize_t n = ::recv(client_fd, buffer.data(), buffer.size() - 1, 0);
```

`recv` reads bytes from the TCP connection.

Return value:

- `n > 0`: read `n` bytes.
- `n == 0`: peer closed the connection.
- `n < 0`: read error.

In blocking mode, if the client has not sent data yet, `recv` may block.

### request string

```cpp
std::string request(buffer.data(), static_cast<std::size_t>(n));
```

This converts exactly the received bytes into a C++ string.

We use `n` instead of relying on `'\0'`, because TCP data is a byte stream and should be handled by length.

### extractPath

```cpp
std::string path = extractPath(request);
```

This extracts the path from the HTTP request line.

Example:

```text
GET /search?q=epoll HTTP/1.1
```

Extracted path:

```text
/search?q=epoll
```

### route

```cpp
std::string response = route(path);
```

`route` maps the request path to an HTTP response.

Current routes:

- `/`
- `/search?...`
- other paths return 404

### writeAll

```cpp
writeAll(client_fd, response);
```

This sends the full HTTP response back to the client.

The response is written to `client_fd`, not `listen_fd`.

## Current Limitations

This function is intentionally simple.

Limitations:

- It reads only once.
- It assumes the request fits in 4096 bytes.
- It does not handle partial HTTP requests.
- It does not support keep-alive.
- It handles only basic GET paths.
- A slow client can block the whole server.

These limitations are useful because they point to the later roadmap:

```text
buffer management -> HTTP parser -> nonblocking IO -> epoll Reactor
```

## Hand-Type Version

Type this version from memory:

```cpp
void handleClient(int client_fd) {
    std::array<char, 4096> buffer {};

    ssize_t n = ::recv(client_fd, buffer.data(), buffer.size() - 1, 0);
    if (n <= 0) {
        return;
    }

    std::string request(buffer.data(), static_cast<std::size_t>(n));
    std::string path = extractPath(request);
    std::string response = route(path);

    writeAll(client_fd, response);
}
```

## Feynman Explanation

`handleClient` is the worker for one client connection.

The server already got `client_fd` from `accept`. `handleClient` uses this fd to read the client's HTTP request bytes. Then it turns those bytes into a request string, extracts the URL path, chooses the matching response through `route`, and sends the response back through the same `client_fd`.

In short:

```text
read what the client wants -> decide what to return -> send it back
```

## Review Questions

1. Why does `handleClient` use `client_fd` instead of `listen_fd`?
2. What does `recv` return when the client closes the connection?
3. Why do we build `std::string request(buffer.data(), n)` instead of just `std::string request(buffer.data())`?
4. What path is extracted from `GET /search?q=epoll HTTP/1.1`?
5. Why is reading only once not robust enough?
6. Why can this function block the whole server in the current model?

## Learned Summary

After learning this function, remember only this:

> `handleClient` handles one connected fd: receive request bytes, parse the path, route to a response, and send the response back through the same `client_fd`.

## 2026-07-08 Learning Check

User's current understanding:

- Correctly understood that the server creates `listen_fd`, builds an address, binds fd and address, and starts listening.
- Correctly understood that `fd` is more like a handle than a data container.
- Correctly understood that request bytes are stored in kernel socket buffers, and `recv` reads bytes from that buffer.
- Correctly understood that `request` is a string representation of received HTTP bytes.
- Correctly understood that `path` is extracted from the request and used by `route` to choose server-side response content.
- Correctly understood that `send`/`writeAll` sends the response back to the client through the same `client_fd`.

Correction:

- A client does not send a `client_fd` into the queue. File descriptors are local integers created by the server process/kernel. What enters the kernel pending queue is a completed TCP connection. When the server calls `accept(listen_fd, ...)`, the kernel returns a new local `client_fd` representing that connection.

Final Chinese summary:

> 服务器先创建 `listen_fd`，绑定 IP/端口并监听。客户端发起 TCP 连接后，完成握手的连接进入内核等待 `accept` 的队列。服务器调用 `accept` 后，内核返回本进程里的 `client_fd`。`client_fd` 是连接句柄，不存数据；请求字节在内核 socket 缓冲区里。`recv` 通过 `client_fd` 把字节读到用户态 buffer，再构造成 `request` 字符串，提取 `path`，由 `route` 生成 HTTP `response`，最后 `writeAll/send` 通过同一个 `client_fd` 发回客户端。
