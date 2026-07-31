#pragma once

#include <string>

namespace http {

std::string okText(const std::string& body);

std::string okJson(const std::string& body);

std::string notFound();

}  // namespace http

