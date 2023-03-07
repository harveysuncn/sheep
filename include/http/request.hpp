#pragma once

#include <cstdint>
#include <vector>
#include <string_view>

namespace sheep { 

namespace http {

struct Request
{

    struct Header
    {
        std::string_view name;
        std::string_view value;
    };

    struct Part
    {
        std::string_view info;
        std::string_view data;
    };

    Header& last_header() {
        return headers[headers.size()-1];
    }

    Part& last_part() {
        return parts[parts.size()-1];
    }

    std::string_view method;
    std::string_view uri;
    std::string_view version;
    std::vector<Header> headers;
    std::string_view content;
    std::size_t content_size;
    bool keep_alive;
    bool is_multipart;
    std::string_view part_boundary;
    std::vector<Part> parts;
};



} // namespace http


} // namespace sheep