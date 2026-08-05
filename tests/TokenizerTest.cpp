#include "search/Tokenizer.h"

#include <iostream>
#include <string_view>
#include <vector>

namespace {

bool expect(bool condition, std::string_view message) {
    if (condition) {
        return true;
    }

    std::cerr << "Test failed: " << message << '\n';
    return false;
}

}  // namespace

int main() {
    const auto tokens = search::tokenize("EPOLL drives EventLoop, C++ and epoll_ctl.");
    const std::vector<std::string> expected = {
        "epoll", "drives", "eventloop", "c++", "and", "epoll_ctl",
    };

    const bool passed = expect(tokens == expected,
                               "technical tokens should be lowercased and preserved");
    if (passed) {
        std::cout << "Tokenizer test passed\n";
        return 0;
    }
    return 1;
}
