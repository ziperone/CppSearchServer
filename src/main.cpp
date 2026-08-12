#include "http/HttpResponse.h"
#include "http/HttpRequest.h"
#include "net/Channel.h"
#include "net/EventLoop.h"
#include "net/Socket.h"
#include "net/TcpConnection.h"
#include "search/SearchApplication.h"

#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

std::string extractPath(const std::string& request) {
    const std::string method = "GET ";
    if (request.rfind(method, 0) != 0) {
        return "/";
    }

    std::size_t path_begin = method.size();
    std::size_t path_end = request.find(' ', path_begin);
    if (path_end == std::string::npos) {
        return "/";
    }

    return request.substr(path_begin, path_end - path_begin);
}

std::string route(const std::string& path) {
    if (path == "/") {
        return http::okText("Hello CppSearchServer\n");
    }

    const std::string search_prefix = "/search?";
    if (path.rfind(search_prefix, 0) == 0) {
        std::string query = path.substr(search_prefix.size());
        return http::okJson("{\"message\":\"search endpoint is reserved for week 5\",\"query\":\"" + query + "\"}\n");
    }

    return http::notFound();
}

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

void serveWithEventLoop(int listen_fd, net::TcpConnection::RequestHandler request_handler) {
    if (net::setNonBlocking(listen_fd) < 0) {
        throw std::runtime_error(std::string("set listen fd nonblocking failed: ") + std::strerror(errno));
    }

    net::EventLoop loop;

    auto listen_channel = std::make_shared<net::Channel>(listen_fd);
    listen_channel->setReadCallback([&loop, listen_fd, request_handler] {
        while (true) {
            sockaddr_storage client_addr {};
            socklen_t client_len = sizeof(client_addr);
            int client_fd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);

            if (client_fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                perror("accept");
                break;
            }

            if (net::setNonBlocking(client_fd) < 0) {
                perror("set client fd nonblocking");
                net::closeFd(client_fd);
                continue;
            }

            auto client_channel = std::make_shared<net::Channel>(client_fd);
            auto connection = std::make_shared<net::TcpConnection>(
                loop,
                client_fd,
                request_handler);
            connection->establish(client_channel);
        }
    });
    listen_channel->setErrorCallback([&loop, listen_fd] {
        loop.removeChannel(listen_fd);
        loop.quit();
    });
    listen_channel->enableReading();
    loop.addChannel(listen_channel);

    loop.loop();
}

[[maybe_unused]] void serveForever(int listen_fd) {
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

std::uint16_t parsePort(int argc, char* argv[]) {
    if (argc < 2) {
        return 8080;
    }
    int port = std::atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        throw std::runtime_error("port must be in range 1-65535");
    }
    return static_cast<std::uint16_t>(port);
}

std::filesystem::path parseDocumentsRoot(int argc, char* argv[]) {
    if (argc < 3) {
        return "data/docs";
    }
    return argv[2];
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        std::uint16_t port = parsePort(argc, argv);
        search::SearchApplication application(parseDocumentsRoot(argc, argv));
        int listen_fd = net::createListenSocket(port);

        std::cout << "CppSearchServer listening on 0.0.0.0:" << port << '\n';
        serveWithEventLoop(listen_fd, [&application](std::string_view request) {
            const search::ApplicationResponse application_response = application.handleRequest(request);
            return net::TcpConnection::ResponseResult{
                std::move(application_response.response),
                application_response.close_after_response};
        });

        net::closeFd(listen_fd);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "fatal: " << ex.what() << '\n';
        return 1;
    }
}
