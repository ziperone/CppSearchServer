#include "search/MarkdownChunker.h"

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
    constexpr std::string_view document = R"(# CppSearchServer
Project introduction.

## Network model
Uses epoll.

### Read events
Uses EPOLLIN.

## Cache model
Uses LRU.
)";

    const auto chunks = search::chunkMarkdown(
        "projects/cpp-search-server.md", "projects", document, 10);

    bool passed = true;
    passed &= expect(chunks.size() == 4, "four heading sections should become chunks");
    if (chunks.size() != 4) {
        return 1;
    }

    passed &= expect(chunks[0].id == 10, "the first id should use first_chunk_id");
    passed &= expect(chunks[2].id == 12, "chunk ids should be continuous");
    passed &= expect(chunks[1].title == "CppSearchServer", "the title should come from H1");
    passed &= expect(chunks[2].heading_path == "CppSearchServer > Network model > Read events",
                     "nested headings should form a path");
    passed &= expect(chunks[3].heading_path == "CppSearchServer > Cache model",
                     "a new H2 should discard the old H2 and H3");
    passed &= expect(chunks[1].raw_text == "Uses epoll.", "the body should belong to its heading");
    passed &= expect(chunks[3].category == "projects", "category should be preserved");
    passed &= expect(chunks[3].source_path == "projects/cpp-search-server.md",
                     "source path should be preserved");

    const auto plain_text_chunks = search::chunkMarkdown(
        "skills/cpp.txt", "skills", "C++ and CMake notes.", 20);
    passed &= expect(plain_text_chunks.size() == 1, "plain text should become one chunk");
    if (plain_text_chunks.size() == 1) {
        passed &= expect(plain_text_chunks[0].id == 20, "plain text should preserve its first id");
        passed &= expect(plain_text_chunks[0].title == "cpp", "plain text title should use the file name");
        passed &= expect(plain_text_chunks[0].raw_text == "C++ and CMake notes.",
                         "plain text should preserve its body");
    }

    if (passed) {
        std::cout << "MarkdownChunker test passed\n";
        return 0;
    }
    return 1;
}
