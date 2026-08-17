#include "net/TcpConnection.h"

#include "net/Channel.h"
#include "net/EventLoop.h"
#include "net/Socket.h"

#include <sys/socket.h>

#include <array>
#include <cerrno>
#include <utility>

namespace net {

TcpConnection::TcpConnection(EventLoop& loop,
                             int fd,
                             RequestHandler request_handler,
                             std::chrono::milliseconds idle_timeout)
    : loop_(loop),
      fd_(fd),
      channel_(),
      input_buffer_(),
      output_buffer_(),
      request_handler_(std::move(request_handler)),
      response_ready_(false),
      close_after_response_(false),
      peer_closed_(false),
      idle_timeout_(idle_timeout),
      last_activity_at_(),
      idle_generation_(0),
      processing_request_(false),
      request_generation_(0) {}

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

    markActivity();
    scheduleIdleCheck(last_activity_at_ + idle_timeout_);
}

void TcpConnection::handleRead() {
    std::array<char, 4096> buffer {};
    bool received_data = false;

    while (true) {
        ssize_t n = ::recv(fd_, buffer.data(), buffer.size(), 0);
        if (n > 0) {
            input_buffer_.append(
                std::string_view(buffer.data(), static_cast<std::size_t>(n)));
            received_data = true;
            continue;
        }

        if (n == 0) {
            peer_closed_ = true;
            break;
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

    if (received_data) {
        markActivity();
    }

    if (!response_ready_ && !processing_request_ && hasCompleteRequest()) {
        processRequest();
        return;
    }

    if (peer_closed_ && !response_ready_ && !processing_request_) {
        close();
    }
}

void TcpConnection::handleWrite() {
    bool sent_data = false;

    while (true) {
        while (!output_buffer_.empty()) {
            std::string_view output = output_buffer_.peek();
            ssize_t n = ::send(fd_, output.data(), output.size(), 0);
            if (n > 0) {
                output_buffer_.retrieve(static_cast<std::size_t>(n));
                sent_data = true;
                continue;
            }

            if (n < 0 && errno == EINTR) {
                continue;
            }

            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                if (sent_data) {
                    markActivity();
                }
                enableWriting();
                return;
            }

            close();
            return;
        }

        disableWriting();
        response_ready_ = false;

        if (close_after_response_) {
            close();
            return;
        }

        if (!processing_request_ && hasCompleteRequest()) {
            processRequest();
            return;
        }

        if (peer_closed_) {
            close();
            return;
        }

        if (sent_data) {
            markActivity();
        }
        return;
    }
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

void TcpConnection::markActivity() {
    last_activity_at_ = std::chrono::steady_clock::now();
    ++idle_generation_;
}

void TcpConnection::scheduleIdleCheck(std::chrono::steady_clock::time_point expires_at) {
    const std::uint64_t expected_generation = idle_generation_;
    std::weak_ptr<TcpConnection> weak_self = weak_from_this();
    loop_.runAt(expires_at, [weak_self, expected_generation] {
        std::shared_ptr<TcpConnection> self = weak_self.lock();
        if (!self) {
            return;
        }
        self->closeIfIdle(expected_generation);
    });
}

void TcpConnection::closeIfIdle(std::uint64_t expected_generation) {
    if (fd_ < 0) {
        return;
    }

    if (expected_generation != idle_generation_) {
        scheduleIdleCheck(last_activity_at_ + idle_timeout_);
        return;
    }

    if (!isIdle()) {
        scheduleIdleCheck(std::chrono::steady_clock::now() + idle_timeout_);
        return;
    }

    close();
}

bool TcpConnection::hasCompleteRequest() const {
    return input_buffer_.peek().find("\r\n\r\n") != std::string_view::npos;
}

bool TcpConnection::isIdle() const {
    return input_buffer_.empty() && output_buffer_.empty() && !response_ready_ && !processing_request_;
}

void TcpConnection::finishRequest(std::uint64_t request_id,ResponseResult result){
    if(fd_ <0){
        return;
    }
    if(!processing_request_){
        return;
    }
    if(request_id != request_generation_){
        return;
    }
    processing_request_ = false;
    output_buffer_.append(result.response);
    close_after_response_ = result.close_after_response;
    response_ready_ = true;
    handleWrite();
}



void TcpConnection::processRequest() {
    std::string_view input = input_buffer_.peek();
    std::size_t request_end = input.find("\r\n\r\n") + 4;
    std::string request(input.substr(0, request_end));
    input_buffer_.retrieve(request_end);

    processing_request_ = true;
    const std::uint64_t request_id = ++request_generation_;
    std::weak_ptr<TcpConnection> weak_self = weak_from_this();
    request_handler_(std::move(request),[weak_self,request_id](ResponseResult result)mutable{
        std::shared_ptr<TcpConnection> self = weak_self.lock();
        if (!self) {
            return;
        }
        self->loop_.queueInLoop(
            [weak_self, request_id, result = std::move(result)]() mutable {
                std::shared_ptr<TcpConnection> connection = weak_self.lock();
                if (!connection) {
                    return;
                }
                connection->finishRequest(request_id, std::move(result));
            }
        );
    });
}

}  // namespace net
