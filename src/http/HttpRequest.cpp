#include "http/HttpRequest.h"

#include <cctype>

namespace {

int hexValue(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

std::optional<std::string> urlDecode(std::string_view encoded) {
    std::string decoded;
    decoded.reserve(encoded.size());

    for (std::size_t index = 0; index < encoded.size(); ++index) {
        const char character = encoded[index];
        if (character == '+') {
            decoded += ' ';
            continue;
        }
        if (character != '%') {
            decoded += character;
            continue;
        }
        if (index + 2 >= encoded.size()) {
            return std::nullopt;
        }

        const int high = hexValue(encoded[index + 1]);
        const int low = hexValue(encoded[index + 2]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }

        decoded += static_cast<char>((high << 4) | low);
        index += 2;
    }

    return decoded;
}

bool parseQueryString(std::string_view query,
                      std::unordered_map<std::string, std::string>& query_params) {
    std::size_t cursor = 0;
    while (cursor < query.size()) {
        std::size_t cursor_end = query.find('&', cursor);
        if (cursor_end == std::string_view::npos) {
            cursor_end = query.size();
        }

        const std::string_view item = query.substr(cursor, cursor_end - cursor);
        const std::size_t equal_pos = item.find('=');
        if (equal_pos != std::string_view::npos && equal_pos != 0) {
            const auto key = urlDecode(item.substr(0, equal_pos));
            const auto value = urlDecode(item.substr(equal_pos + 1));
            if (!key || !value) {
                return false;
            }
            query_params.emplace(*key, *value);
        }

        if (cursor_end == query.size()) {
            break;
        }
        cursor = cursor_end + 1;
    }

    return true;
}

bool equalsIgnoreCase(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index) {
        const auto left_character = static_cast<unsigned char>(left[index]);
        const auto right_character = static_cast<unsigned char>(right[index]);
        if (std::tolower(left_character) != std::tolower(right_character)) {
            return false;
        }
    }
    return true;
}

}  // namespace

namespace http {

std::optional<HttpRequest> parseRequest(std::string_view raw_request) {
    std::size_t line_end = raw_request.find("\r\n");
    if (line_end == std::string_view::npos) {
        return std::nullopt;
    }



    std::string_view request_line = raw_request.substr(0, line_end);
    std::size_t first_space = request_line.find(' ');
    if (first_space == std::string_view::npos) {
        return std::nullopt;
    }

    std::size_t second_space = request_line.find(' ', first_space + 1);
    if (second_space == std::string_view::npos) {
        return std::nullopt;
    }

    HttpRequest request;
    request.method = request_line.substr(0, first_space);

    std::string_view target = request_line.substr(
        first_space + 1,
        second_space - first_space - 1);
    std::size_t query_start = target.find('?');
    if (query_start == std::string_view::npos) {
        request.path = target;
    } else {
        request.path = target.substr(0, query_start);
        if (!parseQueryString(target.substr(query_start + 1), request.query_params)) {
            return std::nullopt;
        }
    }
    std::size_t cursor = line_end + 2;
    while (cursor < raw_request.size()) {
        std::size_t header_end = raw_request.find("\r\n", cursor);
        if (header_end == std::string_view::npos) {
            return std::nullopt;
        }
        std::string_view header_line = raw_request.substr(cursor, header_end - cursor);
        if (header_line.empty()) {
            break;
        }
        std::size_t colon = header_line.find(':');
        if (colon == std::string_view::npos || colon == 0) {
            return std::nullopt;
        }
        std::string_view key = header_line.substr(0, colon);
        std::size_t value_start = colon + 1;
        while (value_start < header_line.size()
               && (header_line[value_start] == ' ' || header_line[value_start] == '\t')) {
            ++value_start;
        }
        std::string_view value = header_line.substr(value_start);
        request.headers.emplace(std::string(key), std::string(value));
        cursor = header_end + 2;
    }
    return request;
}

const std::string* findQueryParam(const HttpRequest& request, std::string_view key) {
    auto it = request.query_params.find(std::string(key));
    if (it == request.query_params.end()) {
        return nullptr;
    }
    return &it->second;
}

const std::string* findHeader(const HttpRequest& request, std::string_view name) {
    for (const auto& [key, value] : request.headers) {
        if (equalsIgnoreCase(key, name)) {
            return &value;
        }
    }
    return nullptr;
}

bool shouldCloseConnection(const HttpRequest& request) {
    const std::string* connection = findHeader(request, "Connection");
    return connection != nullptr && equalsIgnoreCase(*connection, "close");
}

}  // namespace http
