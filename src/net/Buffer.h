#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace net {

class Buffer {
public:
    std::size_t readableBytes() const;
    bool empty() const;

    std::string_view peek() const;
    void append(std::string_view data);
    void retrieve(std::size_t length);
    void retrieveAll();

private:
    std::string data_;
};

}  // namespace net
