#include "search/SearchApplication.h"

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "search/DocumentLoader.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

std::string escapeJson(std::string_view value) {
    std::ostringstream escaped;
    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            escaped << "\\\"";
            break;
        case '\\':
            escaped << "\\\\";
            break;
        case '\b':
            escaped << "\\b";
            break;
        case '\f':
            escaped << "\\f";
            break;
        case '\n':
            escaped << "\\n";
            break;
        case '\r':
            escaped << "\\r";
            break;
        case '\t':
            escaped << "\\t";
            break;
        default:
            if (character < 0x20) {
                escaped << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
                        << static_cast<int>(character) << std::dec << std::setfill(' ');
            } else {
                escaped << static_cast<char>(character);
            }
            break;
        }
    }
    return escaped.str();
}

std::string makeSnippet(std::string_view text) {
    constexpr std::size_t kMaxSnippetLength = 240;
    if (text.size() <= kMaxSnippetLength) {
        return std::string(text);
    }
    return std::string(text.substr(0, kMaxSnippetLength)) + "...";
}

std::string toJson(std::string_view query, const std::vector<search::SearchResult>& results) {
    std::ostringstream output;
    output << "{\"query\":\"" << escapeJson(query) << "\",\"results\":[";

    for (std::size_t index = 0; index < results.size(); ++index) {
        if (index != 0) {
            output << ',';
        }

        const auto& result = results[index];
        const auto& chunk = *result.chunk;
        output << "{\"chunk_id\":" << chunk.id
               << ",\"title\":\"" << escapeJson(chunk.title)
               << "\",\"category\":\"" << escapeJson(chunk.category)
               << "\",\"source_path\":\"" << escapeJson(chunk.source_path)
               << "\",\"heading_path\":\"" << escapeJson(chunk.heading_path)
               << "\",\"snippet\":\"" << escapeJson(makeSnippet(chunk.raw_text))
               << "\",\"matched_terms\":" << result.matched_terms
               << ",\"score\":" << std::fixed << std::setprecision(6) << result.score
               << '}';
    }

    output << "]}\n";
    return output.str();
}

}  // namespace

namespace search {

SearchApplication::SearchApplication(const std::filesystem::path& documents_root)
    : chunks_(loadDocuments(documents_root)), search_service_(chunks_, index_) {
    if (!std::filesystem::is_directory(documents_root)) {
        throw std::runtime_error("documents root is not a directory: " + documents_root.string());
    }

    for (const auto& chunk : chunks_) {
        index_.addChunk(chunk);
    }
}

ApplicationResponse SearchApplication::handleRequest(std::string_view request) const {
    const auto parsed = http::parseRequest(request);
    if (!parsed || parsed->method != "GET") {
        return {http::badRequest("Only complete GET requests are supported\n"), false};
    }

    const bool close_after_response = http::shouldCloseConnection(*parsed);

    if (parsed->path == "/") {
        return {http::okText("Hello CppSearchServer\n", close_after_response), close_after_response};
    }

    if (parsed->path != "/search") {
        return {http::notFound(close_after_response), close_after_response};
    }

    const std::string* query = http::findQueryParam(*parsed, "q");
    if (query == nullptr || query->empty()) {
        return {http::badRequest("The q query parameter is required\n", close_after_response),
                close_after_response};
    }

    return {http::okJson(toJson(*query, search_service_.search(*query, 10)), close_after_response),
            close_after_response};
}

}  // namespace search
