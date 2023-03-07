#pragma once

#include <string_view>
#include <vector>

namespace sheep {

namespace http {


struct Response
{
    struct Header
    {
        std::string_view name;
        std::string_view value;
    };

    struct Chunk
    {
        int size{0}; // chunk size is hex
        std::string_view data;
    };

    int status_code{0};
    std::string_view codestr;
    std::string_view status;
    std::string_view version;
    std::vector<Header> headers;
    int content_length{0}; // content_length is decimal
    std::string_view content;
    bool is_chunked{false};
    std::vector<Chunk> chunks;
};


}

} // namespace sheep