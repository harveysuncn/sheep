#pragma once

#include <string_view>

namespace sheep {

namespace log {

enum LogLevel
{
    TRACE = 0,
    DEBUG,
    INFO,
    WARN,
    ERROR
};

inline std::string_view logLevelString(LogLevel level) {
    using namespace std::literals;
    switch (level) {
    case TRACE:
        return "TRACE"sv;
    case DEBUG:
        return "DEBUG"sv;
    case INFO:
        return "INFOM"sv;
    case WARN:
        return "WARNI"sv;
    case ERROR:
        return "ERROR"sv;
    default:
        return "NONE"sv;
    }
}

} // namespace log

} // namespace sheep