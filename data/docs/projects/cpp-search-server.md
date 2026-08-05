# CppSearchServer

## Problem

The service provides low-latency local keyword search for personal job-preparation documents.

## Network model

The server uses epoll and a Reactor-style EventLoop to dispatch readable and writable socket events.

## HTTP handling

The server accepts GET /search?q=... requests and extracts the q query parameter before invoking the search layer.

## Next step

Markdown documents will be split into citeable chunks before building an inverted index.
