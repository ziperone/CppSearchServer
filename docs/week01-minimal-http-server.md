# Week 01 - Minimal HTTP Server

## Why This Module Exists

Before learning epoll, Reactor, thread pools, and search indexing, we need to understand the smallest possible backend server loop:

1. The operating system gives us a socket file descriptor.
2. We bind that socket to an IP and port.
3. We listen for TCP connections.
4. We accept a client.
5. We read bytes from the client.
6. We write bytes back using HTTP response format.

This module is the root of the whole C++ backend project. Every later optimization still depends on this lifecycle.

## What You Should Hand-Type

You should manually type these functions at least once:

- `setNonBlocking(int fd)`
- `createListenSocket(uint16_t port)`
- `serveForever(int listen_fd)`
- `handleClient(int client_fd)`

The goal is not just to make the code run. The goal is to remember what each system call does and why the order matters.

## Core Concepts

### socket

`socket(AF_INET, SOCK_STREAM, 0)` asks the OS for a TCP socket.

The return value is a file descriptor. In Linux, network connections, files, pipes, and many other resources can all be operated through file descriptors.

### bind

`bind` connects the socket to a local IP and port.

For a server, this means: "I want to receive traffic on this address."

### listen

`listen` turns the socket into a passive listening socket.

After this call, the socket is not used to exchange business data directly. It is used to accept new client connections.

### accept

`accept` returns a new connected socket.

The listening socket is responsible for accepting connections. The connected socket is responsible for reading and writing data for one client.

### read and write

At this stage, we use a simple blocking model:

- `read` waits for client data.
- `write` sends a response.
- `close` ends the connection.

This is simple but not scalable. In Week 2, epoll will replace the blocking wait style.

## Current Verification

Expected behavior:

```bash
curl http://127.0.0.1:8080/
```

Expected response body:

```text
Hello CppSearchServer
```

For search path:

```bash
curl "http://127.0.0.1:8080/search?q=epoll"
```

Expected response body:

```json
{"message":"search endpoint is reserved for week 5","query":"q=epoll"}
```

## Feynman Review

Try explaining this module like this:

"A server first creates a socket, then binds it to a port, then listens. The listening socket is like a front desk. Every time a client comes in, accept creates a new socket for that specific client. In the first week version, the server handles one client at a time: read the HTTP request, build a response string, write it back, and close the connection. This model is easy to understand but cannot handle many slow clients well, so next week we need epoll."

## Review Questions

1. Why does `accept` return a new file descriptor instead of using the listening fd directly?
2. What happens if two programs bind the same port?
3. Why do we set `SO_REUSEADDR`?
4. What is the difference between a TCP connection and an HTTP request?
5. Why is this version not suitable for high concurrency?

