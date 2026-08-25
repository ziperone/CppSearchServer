# Week 01 Mastery Checklist

## Goal

This checklist verifies whether the Week 1 blocking HTTP server has been truly understood.

The key idea:

> Week 1 is not about high concurrency. It is about understanding the minimum server lifecycle from listening socket to HTTP response.

## Must-Know Flow

You should be able to explain this without looking at code:

```text
main
  -> parsePort
  -> createListenSocket
  -> serveForever
  -> accept
  -> handleClient
  -> recv
  -> extractPath
  -> route
  -> writeAll/send
  -> close client_fd
```

## One-Sentence Function Summaries

### createListenSocket

Creates the server entrance:

```text
socket -> setsockopt -> sockaddr_in -> bind -> listen -> return listen_fd
```

### serveForever

Runs the blocking accept loop:

```text
wait on listen_fd -> accept client_fd -> handle client_fd -> close client_fd -> repeat
```

### handleClient

Handles one connected client:

```text
recv request bytes -> build request string -> extract path -> route -> write response
```

### writeAll

Sends the whole response as much as possible:

```text
send remaining bytes -> update sent -> repeat until sent == total
```

### extractPath

Parses the HTTP request line:

```text
GET /search?q=epoll HTTP/1.1 -> /search?q=epoll
```

### route

Chooses the response based on path:

```text
/ -> hello response
/search?... -> placeholder JSON
others -> 404
```

## High-Frequency Interview Questions

### Q1: What is the difference between `listen_fd` and `client_fd`?

Expected answer:

```text
listen_fd is the long-lived server entrance used by accept.
client_fd is a short-lived connected socket returned by accept for one specific TCP connection.
listen_fd accepts connections; client_fd reads and writes data.
```

### Q2: Does `client_fd` store the request data?

Expected answer:

```text
No. fd is only a local handle. The request bytes are in the kernel socket receive buffer. recv reads bytes from that buffer into user-space memory.
```

### Q3: Why does `accept` return a new fd?

Expected answer:

```text
Because the listening fd only represents the server entrance. Each completed TCP connection needs its own connected fd so the server can communicate with that specific client.
```

### Q4: What does `backlog` mean?

Expected answer:

```text
backlog is a capacity hint for the kernel's pending completed connection queue. It is not a timeout, not a lifecycle, and not an application task queue.
```

### Q5: Why do we need `htons(port)`?

Expected answer:

```text
Network protocols use network byte order, which is big-endian. htons converts the port from host byte order to network byte order.
```

### Q6: Why does `writeAll` loop over `send`?

Expected answer:

```text
send may only accept part of the response because the kernel send buffer is limited. writeAll tracks how many bytes have been sent and continues sending the remaining bytes.
```

### Q7: Why is the Week 1 server not high-concurrency?

Expected answer:

```text
It is a single-threaded blocking serial model: accept one client, handle it completely, close it, then accept the next. A slow client can block the entire server.
```

### Q8: Is `extractPath` related to TCP or HTTP?

Expected answer:

```text
It is related to HTTP. TCP only transports bytes. HTTP defines request-line format, and extractPath parses the path from that HTTP request line.
```

## Feynman Full Explanation

Try saying this:

> The server first creates a listening socket, binds it to an IP and port, and calls listen. This listening fd is the server entrance. When a client completes a TCP connection, the connection waits in the kernel accept queue. serveForever calls accept on listen_fd and gets a new client_fd. client_fd is only a handle for this connection, not a data container. handleClient uses recv to read HTTP request bytes from the kernel socket buffer, extracts the path from the HTTP request line, routes the path to a response, and writeAll sends the response back through the same client_fd. Finally, the server closes client_fd and goes back to accept the next connection. This model is simple but blocking, so it cannot handle high concurrency well.

## Pass Standard

Week 1 is considered understood when you can:

- Write the main lifecycle from memory.
- Explain `listen_fd` vs `client_fd`.
- Explain where request bytes actually live.
- Explain why `send` may need a loop.
- Explain why the current model blocks.
- Explain why epoll is the natural next step.

## 2026-07-09 Full Flow Review

User can now correctly explain:

- The server creates a listening fd and binds it to an IP/port.
- `listen_fd` is the server entrance.
- `accept` returns a `client_fd` for one specific connection.
- `handleClient` reads bytes, converts them to a request string, extracts the path, routes it to a response, and sends the response back.
- `client_fd` does not store data. It is a handle to a connection; request bytes are in kernel socket buffers and user-space buffers.
- `send` may need a loop because the kernel send buffer may only accept part of the response each time.
- `extractPath` is HTTP-layer logic, not TCP-layer logic.
- The Week 1 model is not concurrent because it serially handles one client at a time; a slow client can block the whole process.

Corrections to remember:

- `parsePort` does not activate a port. It only parses the port number from command-line arguments. The port is actually used during `bind`.
- The function is `bind`, not `find`.
- `serveForever` does not parse the client request. It repeatedly calls `accept` and then delegates request handling to `handleClient`.
- `accept` does not put `client_fd` into the `listen_fd` queue. The kernel has a pending completed connection queue behind `listen_fd`; `accept` takes one completed connection from that queue and returns a new local `client_fd`.
- `extractPath` parses HTTP request format, not TCP connection format.

Final interview-safe explanation:

> `parsePort` only obtains the port number. `createListenSocket` creates a TCP listening fd, sets address reuse, fills `sockaddr_in`, binds the fd to IP/port, and calls `listen`. `serveForever` then loops on `accept`; when a TCP connection has completed the handshake and is waiting in the kernel accept queue, `accept` returns a local `client_fd`. `handleClient` uses `recv` to read HTTP bytes from the socket buffer, builds a request string, extracts the HTTP path, routes it to a response, and `writeAll` loops over `send` to write the full response back through `client_fd`. Finally, the server closes `client_fd` and waits for the next connection. This model is blocking and serial, so a slow client can delay all later clients.
