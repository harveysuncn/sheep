#pragma once

#include <vector>
#include <string_view>

namespace sheep {

namespace http {

struct Uri
{
    struct Query
    {
        std::string_view name;
        std::string_view value;
    };

    std::string_view scheme;
    std::string_view username;
    std::string_view password;
    std::string_view hostname;
    std::string_view portstr;
    uint16_t port{80};
    std::string_view path;
    std::string_view querystr;
    std::vector<Query> queries;
    std::string_view fragment;
};

}

} // namespace sheep