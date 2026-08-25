#pragma once

#include "net/Buffer.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace observability {
class RequestLatency;
struct RequestTiming;
}  // namespace observability

namespace net {

class Channel;
class EventLoop;

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    struct ResponseResult {
        std::string response;
        bool close_after_response = false;
    };

    using ResponseCallback = std::function<void(ResponseResult)>;
    using RequestTimingPtr = std::shared_ptr<observability::RequestTiming>;
    using RequestHandler = std::function<void(std::string request,
                                              RequestTimingPtr timing,
                                              ResponseCallback complete)>;

    TcpConnection(EventLoop& loop,
                  int fd,
                  RequestHandler request_handler,
                  std::chrono::milliseconds idle_timeout = std::chrono::seconds(60),
                  std::shared_ptr<observability::RequestLatency> latency_metrics = nullptr);

    void establish(const std::shared_ptr<Channel>& channel);

private:
    void handleRead();
    void handleWrite();
    void handleError();
    void close();
    void enableWriting();
    void disableWriting();
    void markActivity();
    void scheduleIdleCheck(std::chrono::steady_clock::time_point expires_at);
    void closeIfIdle(std::uint64_t expected_generation);
    void finishRequest(std::uint64_t request_id,
                       RequestTimingPtr timing,
                       ResponseResult result);
    bool hasCompleteRequest() const;
    bool isIdle() const;
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
    std::chrono::milliseconds idle_timeout_;
    std::chrono::steady_clock::time_point last_activity_at_;
    std::uint64_t idle_generation_;
    bool processing_request_;
    std::uint64_t request_generation_;
    std::shared_ptr<observability::RequestLatency> latency_metrics_;
    RequestTimingPtr active_request_timing_;
};

}  // namespace net
