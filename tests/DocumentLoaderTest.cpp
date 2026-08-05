#include "search/DocumentLoader.h"

#include <filesystem>
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

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: document_loader_test <documents_root>\n";
        return 1;
    }

    const auto chunks = search::loadDocuments(std::filesystem::path(argv[1]));

    bool found_network_chunk = false;
    for (const auto& chunk : chunks) {
        if (chunk.heading_path == "CppSearchServer > Network model") {
            found_network_chunk = true;
            break;
        }
    }

    const bool passed =
        expect(!chunks.empty(), "the document directory should produce chunks") &&
        expect(found_network_chunk, "the sample Markdown document should be split by headings");

    if (passed) {
        std::cout << "DocumentLoader test passed\n";
        return 0;
    }
    return 1;
}
