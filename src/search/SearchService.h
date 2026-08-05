#pragma once

#include "search/DocumentChunk.h"
#include "search/InvertedIndex.h"

#include <cstddef>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace search {

struct SearchResult {
    const DocumentChunk* chunk;
    std::size_t matched_terms;
    double score;
};

class SearchService {
public:
    SearchService(const std::vector<DocumentChunk>& chunks, const InvertedIndex& index);

    std::vector<SearchResult> search(std::string_view query, std::size_t top_k) const;

private:
    const InvertedIndex& index_;
    std::unordered_map<std::size_t, const DocumentChunk*> chunks_by_id_;
    std::size_t document_count_;
};

}  // namespace search
