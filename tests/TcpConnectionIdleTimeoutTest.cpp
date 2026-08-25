#include "net/Channel.h"
#include "net/EventLoop.h"
#include "net/Socket.h"
#include "net/TcpConnection.h"

#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cerrno>
#include <iostream>
#include <memory>

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
    auto channel = std::make_shared<net::Channel>(sockets[0]);
    auto connection = std::make_shared<net::TcpConnection>(
        loop,
        sockets[0],
        [](std::string,
           net::TcpConnection::RequestTimingPtr,
           net::TcpConnection::ResponseCallback) {
        },
        20ms);
    connection->establish(channel);

    loop.runAfter(60ms, [&loop] {
        loop.quit();
    });
    loop.loop();

    char byte {};
    const ssize_t received = ::recv(sockets[1], &byte, sizeof(byte), MSG_DONTWAIT);
    ::close(sockets[1]);

    if (received != 0) {
        std::cerr << "idle connection was not closed, recv result=" << received
                  << " errno=" << errno << '\n';
        return 1;
    }

    std::cout << "TcpConnection idle timeout test passed\n";
    return 0;
}
