#include "net/Socket.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

void throwSystemError(const std::string& message) {
    throw std::runtime_error(message + ": " + std::strerror(errno));
}

}  // namespace

namespace net {

int setNonBlocking(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int createListenSocket(std::uint16_t port, int backlog) {
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        throwSystemError("socket failed");
    }

    int reuse = 1;
    if (::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        closeFd(listen_fd);
        throwSystemError("setsockopt SO_REUSEADDR failed");
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        closeFd(listen_fd);
        throwSystemError("bind failed");
    }

    if (::listen(listen_fd, backlog) < 0) {
        closeFd(listen_fd);
        throwSystemError("listen failed");
    }

    return listen_fd;
}

void closeFd(int fd) {
    if (fd >= 0) {
        ::close(fd);
    }
}

}  // namespace net
