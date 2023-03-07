#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctype.h>
#include <iostream>
#include <string_view>
#include "http/request.hpp"


namespace sheep {

namespace http {

enum class RequestParseResult
{
    InCompleted,
    Completed,
    Error
};

class RequestParser
{
public:
    enum class State
    {
        MethodUnstart,
        MethodStart,
        MethodEnd,

        UriStart,
        UriEnd,

        HttpStart,
        HttpEnd,

        VersionStart,
        VersionEnd,

        VersionEnd_r,
        VersionEnd_rn,
        VersionEnd_rnr,
        VersionEnd_rnrn,

        HeaderStart,
        HeaderNameEnd,
        HeaderNameValueSpace,
        HeaderValueStart,
        Header_r,
        Header_rn,

        Header_rnr,
        Header_rnrn,

        PostDataStart,

        MultipartDataStart,
        MultipartDataStart_,
        BoundaryMatchStart,
        BoundaryMatch_r,
        BoundaryMatch_rn,

        PartInfoStart,
        PartInfo_r,
        PartInfo_rn,
        PartInfo_rnr,
        PartInfo_rnrn,
        PartBodyStart,
        LastBoundaryMatch,
    };

    RequestParseResult parse(Request& req, std::string_view buf)
    {
        state = State::MethodUnstart;

        auto prev_it = buf.begin();
        std::string_view::const_iterator potential_boundary_start;

        std::size_t remaining_content_size = 0;

        for (auto it = buf.begin(); it != buf.end(); ++it)
        {
            auto cur_char = *it;

            switch (state)
            {
                case State::MethodUnstart:
                    if (!isalpha(cur_char)) {
                        return RequestParseResult::Error;
                    }
                    state = State::MethodStart;
                    break;

                case State::MethodStart:
                    if (cur_char == ' ') {
                        req.method = std::string_view{prev_it, it};
                        state = State::MethodEnd;
                    } else if (!isalpha(cur_char)) {
                        return RequestParseResult::Error;
                    }
                    break;
                
                case State::MethodEnd:
                    if (is_http_control(cur_char)) {
                        return RequestParseResult::Error;
                    } else {
                        state = State::UriStart;
                        prev_it = it;
                    }
                    break;

                case State::UriStart:
                    if (cur_char == ' ') {
                        req.uri = std::string_view{prev_it, it};
                        state = State::UriEnd;
                    } else if (is_http_control(cur_char)) {
                        return RequestParseResult::Error;
                    }
                    break;   
                
                case State::UriEnd:
                    if (cur_char == 'H') {
                        state = State::HttpStart;
                        prev_it = it;
                    } else
                        return RequestParseResult::Error;
                    break;
                
                case State::HttpStart:
                    if (cur_char == '/') {
                        if (std::string_view{prev_it, it} == std::string_view{"HTTP"}) {
                            state = State::HttpEnd;
                        } else
                            return RequestParseResult::Error;
                    } else if (!isalpha(cur_char)) {
                        return RequestParseResult::Error;
                    }

                    break;

                case State::HttpEnd:
                    if (isdigit(cur_char)) {
                        state = State::VersionStart;
                        prev_it = it;
                    } else
                        return RequestParseResult::Error;
                    break;
                
                case State::VersionStart:
                    if (cur_char == '\r') {
                        req.version = std::string_view{prev_it, it};
                        state = State::VersionEnd_r;
                    } else if (!isdigit(cur_char) && cur_char != '.') {
                        return RequestParseResult::Error;
                    }
                    break;
                
                case State::VersionEnd_r:
                    if (cur_char == '\n') {
                        state = State::VersionEnd_rn;
                    } else
                        return RequestParseResult::Error;
                    break;
                
                case State::VersionEnd_rn:
                    if (cur_char == '\r') {
                        state = State::VersionEnd_rnr;
                    } else if (!is_http_control(cur_char)) {
                        state = State::HeaderStart;
                        prev_it = it;
                    } else 
                        return RequestParseResult::Error;
                    break;

                case State::VersionEnd_rnr:
                    if (cur_char == '\n') {
                        state = State::VersionEnd_rnrn;
                        return RequestParseResult::Completed;
                    } else 
                        return RequestParseResult::Error;

                case State::HeaderStart:
                    if (cur_char == ':') {
                        Request::Header new_header;
                        new_header.name = std::string_view{prev_it, it};
                        req.headers.push_back(new_header);
                        state = State::HeaderNameEnd;
                    } else if (is_http_control(cur_char)) {
                        return RequestParseResult::Error;
                    }
                    break;

                case State::HeaderNameEnd:
                    if (cur_char == ' ') {
                        state = State::HeaderNameValueSpace;
                    } else if (is_http_control(cur_char)) {
                        return RequestParseResult::Error;
                    } else {
                        // no space behind colon
                        state = State::HeaderValueStart;
                        prev_it = it;
                    }
                    break;
                
                case State::HeaderNameValueSpace:
                    if (!is_http_control(cur_char)) {
                        state = State::HeaderValueStart;
                        prev_it = it;
                    } else 
                        return RequestParseResult::Error;
                    break;
                
                case State::HeaderValueStart:
                    if (cur_char == '\r') {
                        req.last_header().value = std::string_view{prev_it, it};
                        state = State::Header_r;
                        if (req.last_header().name == "Content-Type" && is_multipart_in_header_value(req.last_header().value))
                        {
                            req.is_multipart = true;
                            req.part_boundary = find_boundary(req.last_header().value);
                        } else if (req.last_header().name == "Content-Length")
                        {
                            req.content_size = std::atoi(req.last_header().value.begin());
                            remaining_content_size = req.content_size;
                        }  
                    } else if (is_http_control(cur_char)) {
                        return RequestParseResult::Error;
                    }
                    break;
                
                case State::Header_r:
                    if (cur_char == '\n') {
                        state = State::Header_rn;
                    } else 
                        return RequestParseResult::Error;
                    break;
                
                case State::Header_rn:
                    if (cur_char == '\r') {
                        state = State::Header_rnr;
                    } else if (!is_http_control(cur_char)) {
                        prev_it = it;
                        state = State::HeaderStart;
                    } else 
                        return RequestParseResult::Error;
                    break;

                case State::Header_rnr:
                    if (cur_char == '\n') {
                        state = State::Header_rnrn;
                        if (it + 1 == buf.end())
                            return RequestParseResult::Completed;
                    } else 
                        return RequestParseResult::Error;
                    break;
                
                case State::Header_rnrn:
                    // post data here
                    if (req.method == "POST" || req.method == "PUT")
                    {
                        prev_it = it;
                        if (!req.is_multipart) {
                            --remaining_content_size;
                            state = State::PostDataStart;
                            if (remaining_content_size == 0) {
                                req.content = std::string_view{it, it+1};
                                return RequestParseResult::Completed;
                            }
                        } else if (req.is_multipart && cur_char == '-') {
                            state = State::MultipartDataStart;
                        } else {
                            return RequestParseResult::Error;
                        }
                    } else 
                        return RequestParseResult::Completed;
                    break;

                case State::PostDataStart:
                    --remaining_content_size;
                    if (remaining_content_size == 0) {
                        req.content = std::string_view{prev_it, it+1};
                        return RequestParseResult::Completed;
                    }
                    break;

                case State::MultipartDataStart:
                    if (cur_char == '-') {
                        state = State::MultipartDataStart_;
                    } else 
                        return RequestParseResult::Error;
                    break;
                
                case State::MultipartDataStart_:
                    // prefix: --
                    if (is_http_control(cur_char)) {
                        return RequestParseResult::Error;
                    } else {
                        prev_it = it;
                        state = State::BoundaryMatchStart;
                    }
                    break;
                
                case State::BoundaryMatchStart:
                    if (cur_char == '\r') {
                        std::string_view boundary{prev_it, it};
                        if (boundary == req.part_boundary) {
                            state = State::BoundaryMatch_r;
                        } else {
                            return RequestParseResult::Error;
                        }
                    } else if (is_http_control(cur_char)) {
                        return RequestParseResult::Error;
                    }
                    break;
                
                case State::LastBoundaryMatch:
                    if (cur_char == '\n') {
                        return RequestParseResult::Completed;
                    } else
                        return RequestParseResult::Error;
                    break;

                case State::BoundaryMatch_r:
                    if (cur_char == '\n') {
                        state = State::BoundaryMatch_rn;
                    } else 
                        return RequestParseResult::Error;
                    break;
                
                case State::BoundaryMatch_rn:
                    if (!is_http_control(cur_char)) {
                        prev_it = it;
                        state = State::PartInfoStart;
                    } else 
                        return RequestParseResult::Error;
                    break;
                
                case State::PartInfoStart:
                    if (cur_char == '\r') {
                        state = State::PartInfo_r;
                    } else if (is_http_control(cur_char)) {
                        return RequestParseResult::Error;
                    }
                    break;

                case State::PartInfo_r:
                    if (cur_char == '\n') {
                        state = State::PartInfo_rn;
                    } else 
                        return RequestParseResult::Error;
                    break;
                
                case State::PartInfo_rn:
                    if (cur_char == '\r') {
                        Request::Part part;
                        part.info = std::string_view{prev_it, it-2/* trim \r\n */};
                        req.parts.push_back(part);
                        state = State::PartInfo_rnr;
                    } else if (!is_http_control(cur_char)) {
                        state = State::PartInfoStart;
                    }
                    else 
                        return RequestParseResult::Error;
                    break;
                
                case State::PartInfo_rnr:
                    if (cur_char == '\n') {
                        state = State::PartInfo_rnrn;
                    } else 
                        return RequestParseResult::Error;
                    break;
                
                case State::PartInfo_rnrn:
                    state = State::PartBodyStart;
                    prev_it = it;
                    break;
                
                case State::PartBodyStart:
                    if (cur_char == '\r')
                    {
                        auto boundary_size = req.part_boundary.size();
                        auto potential_boundary_start = it - boundary_size;
                        if (std::string_view{potential_boundary_start, it} == req.part_boundary)
                        {
                            // part body ends with "\r\n--${boundary_string}"
                            // so part_body_end_pos = potential_boundary_start - 4
                            req.last_part().data = std::string_view{prev_it, potential_boundary_start-4};
                            state = State::BoundaryMatch_r;
                        }
                        
                        else if (*(it-1) == '-' && *(it-2) == '-' &&
                            std::string_view{potential_boundary_start-2, it-2} == req.part_boundary)
                        {
                            // in this case, current boundary is the last one
                            // --${boundary_string}--\r\n
                            // so part_body_end_pos = it - 2 - sizeof(boundary_string) - sizeof("\r\n--")
                            req.last_part().data = std::string_view{prev_it, potential_boundary_start-6};
                            state = State::LastBoundaryMatch;
                        }
                    }
                    break;

                default:
                    return RequestParseResult::Error;
            }
        }


        return RequestParseResult::InCompleted;
    }

    State get_state() const { return state;}
private:
    bool is_http_control(int ch) {
        return (ch >=0 && ch <= 31) || ch == 127;
    }

    bool is_multipart_in_header_value(std::string_view val) {
        static constexpr char multipart[] = "multipart/form-data";
        auto pos = val.find(multipart);
        return pos != std::string::npos;
    }

    std::string_view find_boundary(std::string_view val) {
        static constexpr char boundary_tag[] = "boundary=";
        auto pos = val.find(boundary_tag);
        return std::string_view{val.begin()+pos+sizeof(boundary_tag)-1, val.end()};
    }

    State state;
};


} // namespace http

} // namespace sheep