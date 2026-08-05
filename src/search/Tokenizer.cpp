#include "search/Tokenizer.h"

#include <cctype>
#include <utility>

namespace search {
namespace {

bool isTokenCharacter(char character) {
    const unsigned char value = static_cast<unsigned char>(character);
    return std::isalnum(value) || character == '+' || character == '_';
}

}  // namespace

std::vector<std::string> tokenize(std::string_view text) {
    std::vector<std::string> tokens;
    std::string current_token;

    for (const char character : text) {
        if (isTokenCharacter(character)) {
            current_token += static_cast<char>(
                std::tolower(static_cast<unsigned char>(character)));
            continue;
        }

        if (!current_token.empty()) {
            tokens.push_back(std::move(current_token));
            current_token.clear();
        }
    }

    if (!current_token.empty()) {
        tokens.push_back(std::move(current_token));
    }

    return tokens;
}

}  // namespace search
