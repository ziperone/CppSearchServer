#pragma once

#include "net/Buffer.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace net {

class Channel;
class EventLoop;

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    struct ResponseResult {
        std::string response;
        bool close_after_response = false;
    };

    using RequestHandler = std::function<ResponseResult(std::string_view)>;

    TcpConnection(EventLoop& loop, int fd, RequestHandler request_handler);

    void establish(const std::shared_ptr<Channel>& channel);

private:
    void handleRead();
    void handleWrite();
    void handleError();
    void close();
    void enableWriting();
    void disableWriting();

    bool hasCompleteRequest() const;
    void processRequest();

    EventLoop& loop_;
    int fd_;
    std::weak_ptr<Channel> channel_;
    Buffer input_buffer_;
    Buffer output_buffer_;
    RequestHandler request_handler_;
    bool response_ready_;
    bool close_after_response_;
    bool peer_closed_;
};

}  // namespace net
