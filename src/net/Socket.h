#pragma once

#include <cstdint>

namespace net {

int setNonBlocking(int fd);

int createListenSocket(std::uint16_t port, int backlog = 128);

void closeFd(int fd);

}  // namespace net

