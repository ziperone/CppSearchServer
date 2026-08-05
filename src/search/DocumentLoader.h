#pragma once

#include "search/DocumentChunk.h"

#include <filesystem>
#include <vector>

namespace search {

std::vector<DocumentChunk> loadDocuments(const std::filesystem::path& documents_root);

}  // namespace search
