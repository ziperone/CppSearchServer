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
};

std::optional<HttpRequest> parseRequest(std::string_view raw_request);

const std::string* findQueryParam(const HttpRequest& request, std::string_view key);

}  // namespace http
