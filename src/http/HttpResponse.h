#pragma once

#include <string>

namespace http {

std::string okText(const std::string& body, bool close_connection = false);

std::string okJson(const std::string& body, bool close_connection = false);

std::string notFound(bool close_connection = false);

std::string badRequest(const std::string& body, bool close_connection = false);

}  // namespace http
