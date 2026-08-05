#include "search/SearchService.h"

#include "search/Tokenizer.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace {

struct Candidate {
    std::size_t matched_terms = 0;
    double score = 0.0;
};

bool ranksHigher(const search::SearchResult& left, const search::SearchResult& right) {
    if (left.matched_terms != right.matched_terms) {
        return left.matched_terms > right.matched_terms;
    }
    if (left.score != right.score) {
        return left.score > right.score;
    }
    return left.chunk->id < right.chunk->id;
}

}  // namespace

namespace search {

SearchService::SearchService(const std::vector<DocumentChunk>& chunks, const InvertedIndex& index)
    : index_(index), document_count_(chunks.size()) {
    for (const auto& chunk : chunks) {
        chunks_by_id_.emplace(chunk.id, &chunk);
    }
}

std::vector<SearchResult> SearchService::search(std::string_view query, std::size_t top_k) const {
    if (top_k == 0 || document_count_ == 0) {
        return {};
    }

    const auto query_tokens = analyzeTerms(query);
    const std::unordered_set<std::string> unique_terms(
        query_tokens.begin(), query_tokens.end());
    std::unordered_map<std::size_t, Candidate> candidates;

    for (const auto& term : unique_terms) {
        const auto* postings = index_.findTerm(term);
        if (postings == nullptr) {
            continue;
        }

        const double idf = std::log(
            (static_cast<double>(document_count_) + 1.0) /
            (static_cast<double>(postings->size()) + 1.0)) + 1.0;

        for (const auto& posting : *postings) {
            auto& candidate = candidates[posting.chunk_id];
            ++candidate.matched_terms;
            candidate.score += std::log1p(
                static_cast<double>(posting.term_frequency)) * idf;
        }
    }

    std::vector<SearchResult> results;
    results.reserve(candidates.size());
    for (const auto& [chunk_id, candidate] : candidates) {
        const auto chunk_iterator = chunks_by_id_.find(chunk_id);
        if (chunk_iterator == chunks_by_id_.end()) {
            continue;
        }

        results.push_back(SearchResult{
            chunk_iterator->second,
            candidate.matched_terms,
            candidate.score,
        });
    }

    if (results.size() > top_k) {
        std::partial_sort(results.begin(), results.begin() + top_k, results.end(), ranksHigher);
        results.resize(top_k);
    } else {
        std::sort(results.begin(), results.end(), ranksHigher);
    }

    return results;
}

}  // namespace search
