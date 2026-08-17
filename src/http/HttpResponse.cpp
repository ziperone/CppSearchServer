#include "http/HttpResponse.h"

#include <string>

namespace {

std::string response(const std::string& status,
                     const std::string& content_type,
                     const std::string& body,
                     bool close_connection) {
    return "HTTP/1.1 " + status + "\r\n"
        + "Content-Type: " + content_type + "\r\n"
        + "Content-Length: " + std::to_string(body.size()) + "\r\n"
        + "Connection: " + (close_connection ? "close" : "keep-alive") + "\r\n"
        + "\r\n"
        + body;
}

}  // namespace

namespace http {

std::string okText(const std::string& body, bool close_connection) {
    return response("200 OK", "text/plain; charset=utf-8", body, close_connection);
}

std::string okJson(const std::string& body, bool close_connection) {
    return response("200 OK", "application/json; charset=utf-8", body, close_connection);
}

std::string notFound(bool close_connection) {
    return response("404 Not Found", "text/plain; charset=utf-8", "Not Found\n", close_connection);
}

std::string badRequest(const std::string& body, bool close_connection) {
    return response("400 Bad Request", "text/plain; charset=utf-8", body, close_connection);
}

std::string internalServerError(bool close_connection) {
    return response("500 Internal Server Error",
                    "text/plain; charset=utf-8",
                    "Internal Server Error\n",
                    close_connection);
}

}  // namespace http
