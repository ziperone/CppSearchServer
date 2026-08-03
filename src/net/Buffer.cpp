#include "net/Buffer.h"

#include <algorithm>

namespace net {

std::size_t Buffer::readableBytes() const {
    return data_.size();
}

bool Buffer::empty() const {
    return data_.empty();
}

std::string_view Buffer::peek() const {
    return data_;
}

void Buffer::append(std::string_view data) {
    data_.append(data);
}

void Buffer::retrieve(std::size_t length) {
    std::size_t consumed = std::min(length,data_.length());
    data_.erase(0,consumed);
    // 由学习者完成：仅消费已经处理的前 length 个字节。
}

void Buffer::retrieveAll() {
    data_.clear();
}

}  // namespace net
