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
#include <string_view>

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
        [](std::string_view) {
            return net::TcpConnection::ResponseResult{"ok", false};
        },
        40ms);
    connection->establish(channel);

    bool remained_open_past_original_deadline = false;
    loop.runAfter(20ms, [client_fd = sockets[1]] {
        constexpr std::string_view request = "GET / HTTP/1.1\r\n\r\n";
        if (::send(client_fd, request.data(), request.size(), 0) < 0) {
            std::cerr << "send failed\n";
        }
    });
    loop.runAfter(50ms, [client_fd = sockets[1], &remained_open_past_original_deadline] {
        char buffer[16] {};
        const ssize_t received = ::recv(client_fd, buffer, sizeof(buffer), MSG_DONTWAIT);
        remained_open_past_original_deadline = received != 0;
    });
    loop.runAfter(110ms, [&loop] {
        loop.quit();
    });
    loop.loop();

    char byte {};
    const ssize_t final_received = ::recv(sockets[1], &byte, sizeof(byte), MSG_DONTWAIT);
    ::close(sockets[1]);

    if (!remained_open_past_original_deadline || final_received != 0) {
        std::cerr << "idle timeout refresh test failed, still_open="
                  << remained_open_past_original_deadline
                  << " final_recv=" << final_received
                  << " errno=" << errno << '\n';
        return 1;
    }

    std::cout << "TcpConnection idle refresh test passed\n";
    return 0;
}
