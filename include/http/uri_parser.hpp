#pragma once 

#include "http/request_parser.hpp"
#include "http/uri.hpp"
#include <cctype>
#include <cstdlib>

namespace sheep {

namespace http {

enum class UriParseResult
{
    Incompleted,
    Completed,
    Error
};


class UriParser
{
public:
    enum class State
    {
        SchemeStart,
        SchemeEnd,
        SchemeEndSlash,
        SchemeEndSlashSlash,

        UsernameOrHostname,
        PortOrPasswordStart,
        PortOrPassword,
        HostnameStart,
        Hostname,
        Password,

        PortStart,
        Port,
        Path,
        QueryStart,
        Query,

        HashStart,
        Hash,

        NewQueryStart,
        QueryName,
        QueryValueStart,
        QueryValue,

    };

    UriParseResult parse(Uri& uri, std::string_view buf)
    {
        state = State::SchemeStart;
        auto prev_it = std::begin(buf);

        for (auto it = std::begin(buf); it != std::end(buf); ++it)
        {
            auto cur_char = *it;

            switch (state)
            {
                case State::SchemeStart:
                    if (isalnum(cur_char) || cur_char == '+' || cur_char == '-' || cur_char == '.') {
                        
                        continue;
                    } else if (cur_char == ':') {
                        uri.scheme = std::string_view{prev_it, it};
                        state = State::SchemeEnd;
                    } else {
                        return UriParseResult::Error;
                    }
                    break;

                case State::SchemeEnd:
                    if (cur_char == '/') {
                        state = State::SchemeEndSlash;
                    } else if (isalnum(cur_char)) {
                        prev_it = it;
                        state = State::UsernameOrHostname;
                    } else 
                        return UriParseResult::Error;
                    break;
                
                case State::SchemeEndSlash:
                    if (cur_char == '/') {
                        state = State::SchemeEndSlashSlash;
                    } else if (isalnum(cur_char)) {
                        prev_it = it;
                        state = State::UsernameOrHostname;
                    } else 
                        return UriParseResult::Error;
                    break;

                case State::SchemeEndSlashSlash:
                    if (isalnum(cur_char)) {
                        prev_it = it;
                        state = State::UsernameOrHostname;
                    } else 
                        return UriParseResult::Error;
                    break;

                case State::UsernameOrHostname:
                    if (is_unreserved(cur_char) || cur_char == '%') {
                        continue;
                    } else if (cur_char == ':') {
                        uri.username = std::string_view{prev_it, it};
                        state = State::PortOrPasswordStart;
                    } else if (cur_char == '@') {
                        uri.username = std::string_view{prev_it, it};
                        state = State::HostnameStart;
                    } else if (cur_char == '/') {
                        uri.hostname = std::string_view{prev_it, it};
                        prev_it = it;
                        state = State::Path;
                    } else 
                        return UriParseResult::Error;
                    break;
                
                case State::PortOrPasswordStart:
                    if (is_unreserved(cur_char)) {
                        prev_it = it;
                        state = State::PortOrPassword;
                    } else
                        return UriParseResult::Error;
                    break;
                
                case State::PortOrPassword:
                    if (isdigit(cur_char)) {
                        continue;
                    } else if (cur_char == '/') {
                        std::swap(uri.hostname, uri.username);
                        uri.portstr = std::string_view{prev_it, it};
                        if (is_all_digit(uri.portstr)) {
                            uri.port = atoi(uri.portstr.data());
                        } else {
                            return UriParseResult::Error;
                        }
                        prev_it = it;
                        state = State::Path;
                    } else if (isalnum(cur_char) || cur_char == '%') {
                        // password
                        state = State::Password;
                    } else if (cur_char == '@') {
                        uri.password = std::string_view{prev_it, it};
                        state = State::HostnameStart;
                    } else 
                        return UriParseResult::Error;
                    break;

                case State::Password:
                    if (isalnum(cur_char) || cur_char == '%') {
                        continue;
                    } else if (cur_char == '@') {
                        uri.password = std::string_view{prev_it, it};
                        state = State::HostnameStart;
                    } else 
                        return UriParseResult::Error;
                    break;
                
                case State::HostnameStart:
                    if (is_unreserved(cur_char) || cur_char == '%') {
                        prev_it = it;
                        state = State::Hostname;
                        continue;
                    } else 
                        return UriParseResult::Error;
                    break;

                case State::Hostname:
                    if (is_unreserved(cur_char) || cur_char == '%') {
                        continue;
                    } else if (cur_char == ':') {
                        uri.hostname = std::string_view{prev_it, it};
                        state = State::PortStart;
                    } else if (cur_char == '/') {
                        uri.hostname = std::string_view{prev_it, it};
                        prev_it = it;
                        state = State::Path;
                    } else
                        return UriParseResult::Error;
                    break;
                
                case State::PortStart:
                    if (isdigit(cur_char)) {
                        prev_it = it;
                        state = State::Port;
                    } else 
                        return UriParseResult::Error;
                    break;

                case State::Port:
                    if (isdigit(cur_char)) {
                        continue;
                    } else if (cur_char == '/') {
                        uri.portstr = std::string_view{prev_it, it};
                        if (is_all_digit(uri.portstr)) {
                            uri.port = atoi(uri.portstr.data());
                        } else {
                            return UriParseResult::Error;
                        }
                        prev_it = it;
                        state = State::Path;
                    } else 
                        return UriParseResult::Error;
                    break;
                    
                case State::Path:
                    if (cur_char == '#') {
                        uri.path = std::string_view{prev_it, it};
                        state = State::HashStart;
                    } else if (cur_char == '?') {
                        uri.path = std::string_view{prev_it, it};
                        state = State::QueryStart;
                    }
                    break;

                case State::HashStart:
                    uri.fragment = std::string_view{it, std::end(buf)};
                    return parse_queries(uri);

                case State::QueryStart:
                    prev_it = it;
                    state = State::Query;
                    break;

                case State::Query:
                    if (cur_char == '#') {
                        uri.querystr = std::string_view{prev_it, it};
                        state = State::HashStart;
                    }
                    break;
                
                default:
                    return UriParseResult::Error;
            }
        }

        if (uri.path.empty()) {
            uri.path = std::string_view{prev_it, std::end(buf)};
            return UriParseResult::Completed;
        } else if (uri.querystr.empty()) {
            uri.querystr = std::string_view{prev_it, std::end(buf)};
            return parse_queries(uri);
        }

        return UriParseResult::Incompleted;
    }

    UriParseResult parse_queries(Uri& uri) {
        // parse queries
        if (!uri.querystr.empty()) 
        {
            auto prev_it = std::begin(uri.querystr);
            if (!is_unreserved(*prev_it) && *prev_it != '%')
                return UriParseResult::Error;

            state = State::QueryName;
            std::string_view query_name;
            for (auto it = prev_it; it != std::end(uri.querystr); ++it)
            {
                auto cur_char = *it;
                switch (state)
                {
                    case State::QueryName:
                        if (is_unreserved(cur_char) || cur_char == '%') {
                            continue;
                        } else if (cur_char == '=') {
                            query_name = std::string_view{prev_it, it};
                            state = State::QueryValueStart;
                        } else 
                            return UriParseResult::Error;
                        break;
                    
                    case State::QueryValueStart:
                        if (is_unreserved(cur_char) || cur_char == '%') {
                            state = State::QueryValue;
                            prev_it = it;
                        } else 
                            return UriParseResult::Error;
                        break;
                    
                    case State::QueryValue:
                        if (is_unreserved(cur_char) || cur_char == '%') {
                            continue;
                        } else if (cur_char == '&') {
                            uri.queries.emplace_back(Uri::Query{query_name, std::string_view{prev_it, it}});
                            state = State::NewQueryStart;
                        } else
                            return UriParseResult::Error;
                        break;
                    
                    case State::NewQueryStart:
                        if (is_unreserved(cur_char) || cur_char == '%') {
                            prev_it = it;
                            state = State::QueryName;
                        } else
                            return UriParseResult::Error;
                        break;

                    default:
                        return UriParseResult::Error;
                }
            }

            if (state == State::QueryValue) {
                uri.queries.emplace_back(Uri::Query{query_name, std::string_view{prev_it, std::end(uri.querystr)}});
                return UriParseResult::Completed;
            } else {
                return UriParseResult::Error;
            }
        }
        return UriParseResult::Completed;
    }

    static bool is_unreserved(char ch) {
        if (isalnum(ch)) return true;

        switch (ch)
        {
            case '-':
            case '.':
            case '_':
            case '~':
                return true;
        }

        return false;
    }

    static bool is_all_digit(std::string_view str) {
        for (auto ch: str) {
            if (!isdigit(ch)) return false;
        }
        return true;
    }

    State state;
};

}


}