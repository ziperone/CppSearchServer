#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace http {

struct HttpRequest {
    std::string method;
    std::string path;
    std::unordered_map<std::string, std::string> query_params;
    std::unordered_map<std::string, std::string> headers;
};

std::optional<HttpRequest> parseRequest(std::string_view raw_request);

const std::string* findQueryParam(const HttpRequest& request, std::string_view key);

const std::string* findHeader(const HttpRequest& request, std::string_view name);

bool shouldCloseConnection(const HttpRequest& request);

}  // namespace http
