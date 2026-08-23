#pragma once

#include "search/DocumentChunk.h"
#include "search/InvertedIndex.h"
#include "search/SearchCache.h"
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
    SearchApplication(const std::filesystem::path& documents_root,
                      SearchCache::Config cache_config);

    ApplicationResponse handleRequest(std::string_view request) const;
    LruCache::Stats cacheStats() const;

private:
    std::vector<DocumentChunk> chunks_;
    InvertedIndex index_;
    SearchService search_service_;
    mutable SearchCache search_cache_;
};

}  // namespace search
