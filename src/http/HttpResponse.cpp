#include "http/HttpResponse.h"

#include <string>

namespace {

std::string response(const std::string& status,
                     const std::string& content_type,
                     const std::string& body) {
    return "HTTP/1.1 " + status + "\r\n"
        + "Content-Type: " + content_type + "\r\n"
        + "Content-Length: " + std::to_string(body.size()) + "\r\n"
        + "Connection: close\r\n"
        + "\r\n"
        + body;
}

}  // namespace

namespace http {

std::string okText(const std::string& body) {
    return response("200 OK", "text/plain; charset=utf-8", body);
}

std::string okJson(const std::string& body) {
    return response("200 OK", "application/json; charset=utf-8", body);
}

std::string notFound() {
    return response("404 Not Found", "text/plain; charset=utf-8", "Not Found\n");
}

std::string badRequest(const std::string& body) {
    return response("400 Bad Request", "text/plain; charset=utf-8", body);
}

}  // namespace http
