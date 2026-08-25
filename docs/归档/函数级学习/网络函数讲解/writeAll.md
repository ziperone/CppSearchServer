# writeAll

## Function Purpose

`writeAll` sends a full response string through one connected socket fd.

In one sentence:

> It keeps calling `send` until all bytes in the response have been written to `client_fd`, or until sending fails.

## Source Location

- `src/main.cpp`

## Core Flow

```text
data pointer -> total length -> sent offset -> send remaining bytes -> update sent -> repeat
```

## Code

```cpp
void writeAll(int fd, const std::string& data) {
    const char* buffer = data.data();
    std::size_t total = data.size();
    std::size_t sent = 0;

    while (sent < total) {
        ssize_t n = ::send(fd, buffer + sent, total - sent, 0);
        if (n <= 0) {
            return;
        }
        sent += static_cast<std::size_t>(n);
    }
}
```

## Key Points

### fd

Here `fd` is the connected socket fd, usually `client_fd`.

It is not `listen_fd`.

`writeAll` writes response bytes to the client connection.

### data

`data` is the full HTTP response string.

It includes:

```text
status line
headers
blank line
body
```

Example:

```http
HTTP/1.1 200 OK
Content-Type: text/plain; charset=utf-8
Content-Length: 22
Connection: close

Hello CppSearchServer
```

### buffer

```cpp
const char* buffer = data.data();
```

This gets a pointer to the first byte of the response string.

`send` works with raw bytes, so we pass it a pointer and a length.

### total

```cpp
std::size_t total = data.size();
```

`total` is the number of bytes that need to be sent.

Do not use C string length rules here. HTTP response is bytes.

### sent

```cpp
std::size_t sent = 0;
```

`sent` records how many bytes have already been successfully sent.

It is an offset into `buffer`.

### send

```cpp
ssize_t n = ::send(fd, buffer + sent, total - sent, 0);
```

This means:

```text
start from buffer + sent
try to send total - sent bytes
```

`send` may send fewer bytes than requested.

Example:

```text
total = 1000
sent = 0
send returns 400

next time:
start = buffer + 400
remaining = 600
```

### why loop

TCP is a byte stream. The kernel send buffer may not have enough room for all bytes at once.

So one `send` call does not guarantee the full response has been sent.

The loop guarantees:

```text
keep sending until sent == total
```

### n <= 0

If `send` returns `n <= 0`, this simple version stops sending.

Possible reasons:

- client closed connection
- network error
- signal interruption
- in nonblocking mode, send buffer temporarily full

In Week 1 blocking mode, simply returning is acceptable for the baseline.

Later, nonblocking IO will need more careful handling of `EAGAIN`, `EWOULDBLOCK`, and `EINTR`.

## Current Limitation

This function is acceptable for Week 1 blocking IO, but not complete for production.

Limitations:

- It does not retry on `EINTR`.
- It does not handle `EAGAIN` / `EWOULDBLOCK`.
- It does not integrate with epoll write events.
- It does not report whether the full response was sent.

These will matter when we move to nonblocking Reactor.

## Hand-Type Version

Type this version from memory:

```cpp
void writeAll(int fd, const std::string& data) {
    const char* buffer = data.data();
    std::size_t total = data.size();
    std::size_t sent = 0;

    while (sent < total) {
        ssize_t n = ::send(fd, buffer + sent, total - sent, 0);
        if (n <= 0) {
            return;
        }

        sent += static_cast<std::size_t>(n);
    }
}
```

## Feynman Explanation

`writeAll` is used because sending data over TCP is not like copying a string in memory.

The response may be 1000 bytes, but one `send` call might only send 400 bytes. So the function remembers how many bytes have already been sent. Next time, it starts from `buffer + sent` and sends the remaining bytes. It repeats until all response bytes have been sent.

## Review Questions

1. Why can we not assume one `send` sends the whole response?
2. What does `buffer + sent` mean?
3. What does `total - sent` mean?
4. What does `sent += n` record?
5. In this function, should `fd` be `listen_fd` or `client_fd`?
6. Why will this function need changes in nonblocking epoll mode?

## Learned Summary

After learning this function, remember only this:

> `writeAll` loops over `send` because TCP may only accept part of the response each time; `sent` tracks progress until all response bytes are written to `client_fd`.

## 2026-07-08 Learning Check

User's current understanding:

- Correctly understood that the kernel send buffer is limited, so one `send` call may not send the whole response.
- Correctly understood that `buffer + sent` means the next send starts after the bytes already sent, avoiding duplicate sending.
- Correctly understood that `total - sent` means the number of bytes still remaining.
- Correctly understood that this data is returned to the client through the client-specific connection represented by `client_fd`.

Final Chinese summary:

> `writeAll` 是为了保证 HTTP response 尽量完整写回客户端。因为内核发送缓冲区有限，`send` 一次可能只接受部分字节，所以用 `sent` 记录已经发送的长度。每轮从 `buffer + sent` 开始，发送 `total - sent` 个剩余字节，直到全部发送完成或遇到错误。这里的 fd 应该是 `client_fd`，因为它代表当前客户端连接。
