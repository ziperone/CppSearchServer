#include "net/TcpConnection.h"

#include "net/Channel.h"
#include "net/EventLoop.h"
#include "net/Socket.h"

#include <sys/socket.h>

#include <array>
#include <cerrno>
#include <utility>

namespace net {

TcpConnection::TcpConnection(EventLoop& loop, int fd, RequestHandler request_handler)
    : loop_(loop),
      fd_(fd),
      channel_(),
      input_buffer_(),
      output_buffer_(),
      request_handler_(std::move(request_handler)),
      response_ready_(false) {}

void TcpConnection::establish(const std::shared_ptr<Channel>& channel) {
    channel_ = channel;

    std::shared_ptr<TcpConnection> self = shared_from_this();
    channel->setReadCallback([self] {
        self->handleRead();
    });
    channel->setWriteCallback([self] {
        self->handleWrite();
    });
    channel->setErrorCallback([self] {
        self->handleError();
    });
    channel->enableReading();
    loop_.addChannel(channel);
}

void TcpConnection::handleRead() {
    std::array<char, 4096> buffer {};

    while (true) {
        ssize_t n = ::recv(fd_, buffer.data(), buffer.size(), 0);
        if (n > 0) {
            input_buffer_.append(
                std::string_view(buffer.data(), static_cast<std::size_t>(n)));
            continue;
        }

        if (n == 0) {
            if(!response_ready_){
                if(hasCompleteRequest()){
                    processRequest();
                }else{
                    close();
                }
            }
            return;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }

        close();
        return;
    }

    if (!response_ready_ && hasCompleteRequest()) {
        processRequest();
    }
}

void TcpConnection::handleWrite() {
    while (!output_buffer_.empty()) {
        std::string_view output = output_buffer_.peek();
        ssize_t n = ::send(fd_, output.data(), output.size(), 0);
        if (n > 0) {
            output_buffer_.retrieve(static_cast<std::size_t>(n));
            continue;
        }

        if (n < 0 && errno == EINTR) {
            continue;
        }

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            enableWriting();
            return;
        }

        close();
        return;
    }

    disableWriting();
    close();
}

void TcpConnection::handleError() {
    close();
}

void TcpConnection::close() {
    if (fd_ < 0) {
        return;
    }

    loop_.removeChannel(fd_);
    closeFd(fd_);
    fd_ = -1;
}

void TcpConnection::enableWriting() {
    std::shared_ptr<Channel> channel = channel_.lock();
    if (!channel) {
        return;
    }

    channel->enableWriting();
    loop_.updateChannel(channel);
}

void TcpConnection::disableWriting() {
    std::shared_ptr<Channel> channel = channel_.lock();
    if (!channel) {
        return;
    }

    channel->disableWriting();
    loop_.updateChannel(channel);
}

bool TcpConnection::hasCompleteRequest() const {
    return input_buffer_.peek().find("\r\n\r\n") != std::string_view::npos;
}

void TcpConnection::processRequest() {
    std::string_view input = input_buffer_.peek();
    std::size_t request_end = input.find("\r\n\r\n") + 4;
    std::string request(input.substr(0, request_end));
    input_buffer_.retrieve(request_end);

    std::string response = request_handler_(request);
    output_buffer_.append(response);
    response_ready_ = true;
    handleWrite();
}

}  // namespace net
