#pragma once

#include "search/DocumentChunk.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace search {

struct Posting {
    std::size_t chunk_id;
    std::size_t term_frequency;
};

class InvertedIndex {
public:
    void addChunk(const DocumentChunk& chunk);

    const std::vector<Posting>* findTerm(std::string_view normalized_term) const;

private:
    std::unordered_map<std::string, std::vector<Posting>> postings_by_term_;
};

}  // namespace search
