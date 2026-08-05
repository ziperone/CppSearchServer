#include "search/DocumentLoader.h"

#include "search/MarkdownChunker.h"

#include <fstream>
#include <sstream>
#include <string>

namespace search {
namespace {

bool isSupportedDocument(const std::filesystem::path& path) {
    const std::string extension = path.extension().string();
    return extension == ".md" || extension == ".txt";
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        return {};
    }

    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

}  // namespace

std::vector<DocumentChunk> loadDocuments(const std::filesystem::path& documents_root) {
    std::vector<DocumentChunk> chunks;
    std::size_t next_chunk_id = 0;

    if (!std::filesystem::exists(documents_root)) {
        return chunks;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(documents_root)) {
        if (!entry.is_regular_file() || !isSupportedDocument(entry.path())) {
            continue;
        }

        const std::string content = readFile(entry.path());
        if (content.empty()) {
            continue;
        }

        const std::filesystem::path relative_path = std::filesystem::relative(entry.path(), documents_root);
        const std::string category = relative_path.has_parent_path()
            ? relative_path.begin()->string()
            : "uncategorized";

        auto document_chunks = chunkMarkdown(relative_path.generic_string(), category, content, next_chunk_id);
        next_chunk_id += document_chunks.size();
        chunks.insert(chunks.end(), document_chunks.begin(), document_chunks.end());
    }

    return chunks;
}

}  // namespace search
