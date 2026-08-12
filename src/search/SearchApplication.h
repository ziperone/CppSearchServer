#pragma once

#include "search/DocumentChunk.h"
#include "search/InvertedIndex.h"
#include "search/SearchService.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace search {

struct ApplicationResponse {
    std::string response;
    bool close_after_response = false;
};

class SearchApplication {
public:
    explicit SearchApplication(const std::filesystem::path& documents_root);

    ApplicationResponse handleRequest(std::string_view request) const;

private:
    std::vector<DocumentChunk> chunks_;
    InvertedIndex index_;
    SearchService search_service_;
};

}  // namespace search
