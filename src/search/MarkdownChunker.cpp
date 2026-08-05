#include "search/MarkdownChunker.h"

#include <cctype>
#include <sstream>
#include <string>
#include <utility>

namespace {

std::size_t headingLevel(const std::string& line) {
    std::size_t level = 0;
    while (level < line.size() && line[level] == '#') {
        ++level;
    }

    if (level == 0 || level > 6 || level == line.size() || line[level] != ' ') {
        return 0;
    }

    return level;
}

std::string trim(std::string value) {
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(begin, end - begin);
}

std::string joinHeadings(const std::vector<std::string>& headings) {
    std::string path;
    for (const auto& heading : headings) {
        if (heading.empty()) {
            continue;
        }

        if (!path.empty()) {
            path += " > ";
        }
        path += heading;
    }
    return path;
}

std::string plainTextTitle(std::string_view source_path) {
    const std::size_t slash = source_path.find_last_of('/');
    const std::size_t name_begin = slash == std::string_view::npos ? 0 : slash + 1;
    const std::size_t dot = source_path.find_last_of('.');
    const std::size_t name_end = dot == std::string_view::npos || dot < name_begin
        ? source_path.size()
        : dot;
    return std::string(source_path.substr(name_begin, name_end - name_begin));
}

}  // namespace

namespace search {

std::vector<DocumentChunk> chunkMarkdown(
    std::string_view source_path,
    std::string_view category,
    std::string_view content,
    std::size_t first_chunk_id) {
        std::vector<DocumentChunk> chunks;
        std::string doucument_title;
        std::vector<std::string> headings;
        std::string current_body;
        auto flushCurrent = [&]{
            current_body = trim(current_body);
            if(current_body.empty() || headings.empty()){
                return;
            }
            DocumentChunk chunk;
            chunk.id = first_chunk_id + chunks.size();
            chunk.heading_path = joinHeadings(headings);
            chunk.source_path = std::string(source_path);
            chunk.category = std::string(category);
            chunk.title = doucument_title;
            chunk.raw_text = std::move(current_body);
            chunks.push_back(std::move(chunk));
            current_body.clear();
        };
        std::istringstream input{std::string(content)};
        std::string line;
        while(std::getline(input,line)){
            const std::size_t level = headingLevel(line);
            if(level == 0){
                current_body += line;
                current_body += '\n';
                continue;
            }
            flushCurrent();
            const std::string heading = trim(line.substr(level + 1));
            if(headings.size() >= level){
                headings.resize(level - 1);
            }
            headings.push_back(heading);
            if(level == 1){
                doucument_title = heading;
            }
        }
        flushCurrent();

        if (headings.empty()) {
            const std::string body = trim(current_body);
            if (!body.empty()) {
                const std::string title = plainTextTitle(source_path);
                chunks.push_back(DocumentChunk{
                    first_chunk_id,
                    std::string(source_path),
                    std::string(category),
                    title,
                    title,
                    body,
                });
            }
        }
        return chunks;
    }

}  // namespace search
