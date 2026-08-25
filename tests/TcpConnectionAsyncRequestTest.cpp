#include "concurrency/WorkerPool.h"
#include "net/Channel.h"
#include "net/EventLoop.h"
#include "net/Socket.h"
#include "net/TcpConnection.h"
#include "observability/RequestLatency.h"

#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cerrno>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

int main() {
    using namespace std::chrono_literals;

    int sockets[2] {};
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
        std::cerr << "socketpair failed\n";
        return 1;
    }

    if (net::setNonBlocking(sockets[0]) < 0) {
        std::cerr << "setNonBlocking failed\n";
        ::close(sockets[0]);
        ::close(sockets[1]);
        return 1;
    }

    net::EventLoop loop;
    concurrency::WorkerPool workers(2);
    auto latency_metrics = std::make_shared<observability::RequestLatency>(true);
    auto channel = std::make_shared<net::Channel>(sockets[0]);
    auto connection = std::make_shared<net::TcpConnection>(
        loop,
        sockets[0],
        [&workers, latency_metrics](std::string request,
                   net::TcpConnection::RequestTimingPtr timing,
                   net::TcpConnection::ResponseCallback complete) {
            workers.submit([latency_metrics,
                            request = std::move(request),
                            timing = std::move(timing),
                            complete = std::move(complete)]() mutable {
                latency_metrics->markWorkerStarted(timing);
                if (request.find("/first") != std::string::npos) {
                    std::this_thread::sleep_for(30ms);
                    latency_metrics->markWorkerFinished(timing);
                    complete({"first", false});
                    return;
                }

                latency_metrics->markWorkerFinished(timing);
                complete({"second", true});
            });
        },
        500ms,
        latency_metrics);
    connection->establish(channel);

    constexpr std::string_view requests =
        "GET /first HTTP/1.1\r\n\r\n"
        "GET /second HTTP/1.1\r\n\r\n";
    if (::send(sockets[1], requests.data(), requests.size(), 0) < 0) {
        std::cerr << "send requests failed\n";
        ::close(sockets[1]);
        return 1;
    }

    std::string received;
    loop.runAfter(150ms, [&loop, client_fd = sockets[1], &received] {
        char buffer[64] {};
        while (true) {
            const ssize_t n = ::recv(client_fd, buffer, sizeof(buffer), MSG_DONTWAIT);
            if (n > 0) {
                received.append(buffer, static_cast<std::size_t>(n));
                continue;
            }

            if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                std::cerr << "recv response failed\n";
            }
            break;
        }
        loop.quit();
    });
    loop.loop();

    ::close(sockets[1]);

    if (received != "firstsecond") {
        std::cerr << "async response order test failed, received='" << received << "'\n";
        return 1;
    }

    const std::string metrics = latency_metrics->snapshotJson();
    if (metrics.find("\"completed_requests\":2") == std::string::npos) {
        std::cerr << "async metrics test failed, metrics='" << metrics << "'\n";
        return 1;
    }

    std::cout << "TcpConnection async request test passed\n";
    return 0;
}
