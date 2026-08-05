#include "search/Tokenizer.h"

#include <cctype>
#include <unordered_set>
#include <utility>

namespace search {
namespace {

bool isTokenCharacter(char character) {
    const unsigned char value = static_cast<unsigned char>(character);
    return std::isalnum(value) || character == '+' || character == '_';
}

const std::unordered_set<std::string>& stopWords() {
    static const std::unordered_set<std::string> words = {
        "a", "an", "and", "are", "as", "at", "be", "by", "for", "from",
        "in", "is", "of", "on", "or", "the", "to", "uses", "with",
    };
    return words;
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

std::vector<std::string> analyzeTerms(std::string_view text) {
    auto tokens = tokenize(text);
    std::vector<std::string> terms;
    terms.reserve(tokens.size());

    for (auto& token : tokens) {
        if (stopWords().find(token) == stopWords().end()) {
            terms.push_back(std::move(token));
        }
    }

    return terms;
}

}  // namespace search
