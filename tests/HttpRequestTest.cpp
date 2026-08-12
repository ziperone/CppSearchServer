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
    const auto headers = http::parseRequest(
        "GET / HTTP/1.1\r\n"
        "connection:CLOSE\r\n"
        "Accept:\tapplication/json\r\n"
        "\r\n");

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
    passed &= expect(headers.has_value(), "a request without query parameters should parse headers");
    if (headers) {
        const auto connection = headers->headers.find("Connection");
        const auto accept = headers->headers.find("Accept");
        passed &= expect(headers->path == "/", "a request without a query should retain its path");
        passed &= expect(connection == headers->headers.end(),
                         "headers should preserve the original field-name spelling");
        passed &= expect(http::findHeader(*headers, "Connection") != nullptr,
                         "header lookup should ignore field-name case");
        passed &= expect(http::shouldCloseConnection(*headers),
                         "Connection close should ignore field-name and value case");
        passed &= expect(*http::findHeader(*headers, "Connection") == "CLOSE",
                         "a header value without a space after colon should parse");
        passed &= expect(accept != headers->headers.end() && accept->second == "application/json",
                         "leading tab before a header value should be ignored");
    }

    if (passed) {
        std::cout << "HttpRequest test passed\n";
        return 0;
    }
    return 1;
}
