#pragma once

#include <cstddef>
#include <string>

namespace search {

// A citeable section extracted from one local document.
struct DocumentChunk {
    std::size_t id;
    std::string source_path;
    std::string category;
    std::string title;
    std::string heading_path;
    std::string raw_text;
};

}  // namespace search
