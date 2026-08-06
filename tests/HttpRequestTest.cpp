#include "http/HttpRequest.h"

#include <iostream>
#include <string_view>

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
    const auto decoded = http::parseRequest(
        "GET /search?q=event%20loop&language=C%2B%2B&note=a+b HTTP/1.1\r\n\r\n");
    const auto malformed = http::parseRequest(
        "GET /search?q=event%2 HTTP/1.1\r\n\r\n");

    bool passed = true;
    passed &= expect(decoded.has_value(), "a valid percent-encoded query should parse");
    if (decoded) {
        const std::string* query = http::findQueryParam(*decoded, "q");
        const std::string* language = http::findQueryParam(*decoded, "language");
        const std::string* note = http::findQueryParam(*decoded, "note");
        passed &= expect(query != nullptr && *query == "event loop", "%20 should decode to a space");
        passed &= expect(language != nullptr && *language == "C++", "%2B should decode to plus");
        passed &= expect(note != nullptr && *note == "a b", "+ should decode to a space");
    }
    passed &= expect(!malformed.has_value(), "an incomplete percent escape should be rejected");

    if (passed) {
        std::cout << "HttpRequest test passed\n";
        return 0;
    }
    return 1;
}
