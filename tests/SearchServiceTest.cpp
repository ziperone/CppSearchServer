#include "search/InvertedIndex.h"
#include "search/SearchService.h"

#include <iostream>
#include <string_view>
#include <vector>

namespace {

bool expect(bool condition, std::string_view message) {
    if (condition) {
        return true;
    }

    std::cerr << "Test failed: " << message << '\n';
    return false;
}

}  // namespace

int main() {
    const std::vector<search::DocumentChunk> chunks = {
        {0, "projects/a.md", "projects", "A", "A", "epoll epoll epoll epoll epoll"},
        {1, "projects/b.md", "projects", "B", "B", "epoll reactor"},
        {2, "knowledge/c.md", "knowledge", "C", "C", "reactor"},
    };

    search::InvertedIndex index;
    for (const auto& chunk : chunks) {
        index.addChunk(chunk);
    }

    const search::SearchService service(chunks, index);
    const auto results = service.search("EPOLL epoll reactor and", 2);

    bool passed = true;
    passed &= expect(results.size() == 2, "top_k should limit the result count");
    if (results.size() == 2) {
        passed &= expect(results[0].chunk->id == 1,
                         "the chunk matching both query terms should rank first");
        passed &= expect(results[0].matched_terms == 2,
                         "duplicate query terms should not inflate coverage");
        passed &= expect(results[1].chunk->id == 0,
                         "the high-frequency partial match should rank after full coverage");
    }

    passed &= expect(service.search("not_exist_term", 10).empty(),
                     "a missing term should return an empty result list");

    if (passed) {
        std::cout << "SearchService test passed\n";
        return 0;
    }
    return 1;
}
