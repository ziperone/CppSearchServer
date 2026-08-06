#include "search/SearchApplication.h"

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

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: search_application_test <documents_root>\n";
        return 1;
    }

    const search::SearchApplication application(argv[1]);
    const std::string found = application.handleRequest(
        "GET /search?q=epoll HTTP/1.1\r\nHost: localhost\r\n\r\n");
    const std::string multi_term = application.handleRequest(
        "GET /search?q=epoll%20reactor HTTP/1.1\r\nHost: localhost\r\n\r\n");
    const std::string missing = application.handleRequest(
        "GET /search?q=not_exist_term HTTP/1.1\r\nHost: localhost\r\n\r\n");
    const std::string invalid = application.handleRequest(
        "GET /search HTTP/1.1\r\nHost: localhost\r\n\r\n");
    const std::string malformed = application.handleRequest(
        "GET /search?q=bad%2 HTTP/1.1\r\nHost: localhost\r\n\r\n");

    const bool passed =
        expect(found.find("HTTP/1.1 200 OK") == 0, "a matching query should return 200") &&
        expect(found.find("\"heading_path\":\"CppSearchServer > Network model\"") != std::string::npos,
               "a matching query should contain a real document chunk") &&
        expect(multi_term.find("\"query\":\"epoll reactor\"") != std::string::npos,
               "percent-encoded query terms should be decoded before searching") &&
        expect(multi_term.find("\"matched_terms\":2") != std::string::npos,
               "decoded multi-term query should use the real search terms") &&
        expect(missing.find("\"results\":[]") != std::string::npos,
               "a missing term should return an empty JSON result array") &&
        expect(invalid.find("HTTP/1.1 400 Bad Request") == 0,
               "a request without q should return 400") &&
        expect(malformed.find("HTTP/1.1 400 Bad Request") == 0,
               "malformed URL encoding should return 400");

    if (passed) {
        std::cout << "SearchApplication test passed\n";
        return 0;
    }
    return 1;
}
