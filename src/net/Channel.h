#pragma once

#include <cstdint>
#include <functional>

namespace net {

class Channel {
public:
    using EventCallback = std::function<void()>;

    explicit Channel(int fd);

    int fd() const;
    std::uint32_t events() const;

    void enableReading();
    void enableWriting();
    void disableWriting();
    void disableAll();

    void setReadCallback(EventCallback callback);
    void setWriteCallback(EventCallback callback);
    void setErrorCallback(EventCallback callback);

    void handleEvent(std::uint32_t revents);

private:
    int fd_;
    std::uint32_t events_;
    EventCallback read_callback_;
    EventCallback write_callback_;
    EventCallback error_callback_;
};

}  // namespace net
