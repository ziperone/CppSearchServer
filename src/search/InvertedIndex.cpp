#include "search/InvertedIndex.h"

#include "search/Tokenizer.h"

namespace search {

void InvertedIndex::addChunk(const DocumentChunk& chunk) {
    const auto tokens = tokenize(chunk.raw_text);
    std::unordered_map<std::string, std::size_t> term_frequencies;
    for (auto &token : tokens) {
        ++term_frequencies[token];
    }
    for (auto &pair : term_frequencies) {
        const std::string& term = pair.first;
        std::size_t frequency = pair.second;
        postings_by_term_[term].push_back(Posting{chunk.id, frequency});
    }

}

const std::vector<Posting>* InvertedIndex::findTerm(std::string_view normalized_term) const {
    const auto iterator = postings_by_term_.find(std::string(normalized_term));
    if (iterator == postings_by_term_.end()) {
        return nullptr;
    }

    return &iterator->second;
}

}  // namespace search
