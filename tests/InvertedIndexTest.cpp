#include "search/InvertedIndex.h"

#include <iostream>
#include <string_view>

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
    search::InvertedIndex index;

    const search::DocumentChunk first_chunk{
        0, "projects/a.md", "projects", "A", "A > Network", "EPOLL uses epoll.",
    };
    const search::DocumentChunk second_chunk{
        1, "projects/b.md", "projects", "B", "B > Loop", "EventLoop uses epoll.",
    };

    index.addChunk(first_chunk);
    index.addChunk(second_chunk);

    const auto* epoll_postings = index.findTerm("epoll");
    const auto* eventloop_postings = index.findTerm("eventloop");

    bool passed = true;
    passed &= expect(epoll_postings != nullptr, "epoll should exist in the index");
    if (epoll_postings != nullptr) {
        passed &= expect(epoll_postings->size() == 2,
                         "epoll should have one posting per chunk");
        if (epoll_postings->size() == 2) {
            passed &= expect((*epoll_postings)[0].chunk_id == 0,
                             "the first epoll posting should reference chunk 0");
            passed &= expect((*epoll_postings)[0].term_frequency == 2,
                             "epoll appears twice in chunk 0");
            passed &= expect((*epoll_postings)[1].chunk_id == 1,
                             "the second epoll posting should reference chunk 1");
            passed &= expect((*epoll_postings)[1].term_frequency == 1,
                             "epoll appears once in chunk 1");
        }
    }

    passed &= expect(eventloop_postings != nullptr,
                     "EventLoop should be normalized before indexing");

    if (passed) {
        std::cout << "InvertedIndex test passed\n";
        return 0;
    }
    return 1;
}
