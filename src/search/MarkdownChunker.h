#pragma once

#include "search/DocumentChunk.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace search {

std::vector<DocumentChunk> chunkMarkdown(
    std::string_view source_path,
    std::string_view category,
    std::string_view content,
    std::size_t first_chunk_id);

}  // namespace search
