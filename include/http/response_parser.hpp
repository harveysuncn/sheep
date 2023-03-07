#pragma once

#include "http/response.hpp"
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>
namespace sheep {

namespace http {


enum class ResponseParseResult
{
    InCompleted,
    Completed,
    Error
};

class ResponseParser
{
public:
    enum class State
    {
        StatusStart,
        StatusStart_h,
        StatusStart_ht,
        StatusStart_htt,
        StatusStart_http,
        HttpEnd,
        VersionStart,
        VersionEnd,
        StatusCodeStart,
        StatusCodeEnd,
        StatusMsgStart,
        StatusMsg,
        StatusMsgEnd,
        StatusMsgEnd_rn,
        StatusMsgEnd_rnr,

        HeaderName,
        HeaderNameEnd,
        HeaderNameValueSpace,
        HeaderValueStart,
        Header_r,
        Header_rn,
        Header_rnr,

        DataStart,

        NewChunkStart,
        ChunkDataSize,
        ChunkDataSize_r,
        ChunkDataSize_rn,
        ChunkDataEnd,
        ChunkData_r,
        ZeroChunkSize_r,
    };

    ResponseParseResult parse(Response& res, std::string_view buf) 
    {
        state = State::StatusStart;
        auto prev_it = std::begin(buf);
        std::string_view header_name;
        int content_length = 0;
        int new_chunk_size = 0;
        for (auto it = std::begin(buf); it != std::end(buf); ++it)
        {
            auto cur_char = *it;
            switch (state)
            {
                case State::StatusStart:
                    if (cur_char != 'H')
                        return ResponseParseResult::Error;
                    else 
                        state = State::StatusStart_h;
                    break;
                
                case State::StatusStart_h:
                    if (cur_char == 'T') {
                        state = State::StatusStart_ht;
                    } else
                        return ResponseParseResult::Error;
                    break;
                
                case State::StatusStart_ht:
                    if (cur_char == 'T') {
                        state = State::StatusStart_htt;
                    } else 
                        return ResponseParseResult::Error;
                    break;

                case State::StatusStart_htt:
                    if (cur_char == 'P') {
                        state = State::StatusStart_http;
                    } else 
                        return ResponseParseResult::Error;
                    break;

                case State::StatusStart_http:
                    if (cur_char == '/') {
                        state = State::HttpEnd;
                    } else 
                        return ResponseParseResult::Error;
                    break;

                case State::HttpEnd:
                    if (isdigit(cur_char)) {
                        state = State::VersionStart;
                        prev_it = it;
                    } else
                        return ResponseParseResult::Error;
                    break;

                case State::VersionStart:
                    if (isdigit(cur_char) || cur_char == '.') {
                        continue;
                    } else if (cur_char == ' ') {
                        res.version = std::string_view{prev_it, it};
                        state = State::VersionEnd;
                    } else 
                        return ResponseParseResult::Error;
                    break;

                case State::VersionEnd:
                    if (isdigit(cur_char)) {
                        prev_it = it;
                        state = State::StatusCodeStart;
                    } else 
                        return ResponseParseResult::Error;
                    break;
                
                case State::StatusCodeStart:
                    if (isdigit(cur_char)) {
                        continue;
                    } else if (cur_char == ' ') {
                        res.codestr = std::string_view{prev_it, it};
                        if (is_all_digit(res.codestr)) {
                            res.status_code = atoi(res.codestr.data());
                            state = State::StatusCodeEnd;
                        } else {
                            return ResponseParseResult::Error;
                        }
                    } else
                        return ResponseParseResult::Error;
                    break;

                case State::StatusCodeEnd:
                    if (!is_http_control(cur_char)) {
                        state = State::StatusMsgStart;
                        prev_it = it;
                    } else {
                        return ResponseParseResult::Error;
                    }
                    break;

                case State::StatusMsgStart:
                    if (!is_http_control(cur_char)) {
                        state = State::StatusMsg;
                    } else
                        return ResponseParseResult::Error;
                    break;

                case State::StatusMsg:
                    if (cur_char == '\r') {
                        res.status = std::string_view{prev_it, it};
                        state = State::StatusMsgEnd;
                    } else if (is_http_control(cur_char))
                        return ResponseParseResult::Error;
                    break;

                case State::StatusMsgEnd:
                    if (cur_char == '\n') {
                        state = State::StatusMsgEnd_rn;
                    } else 
                        return ResponseParseResult::Error;
                    break;
                
                case State::StatusMsgEnd_rn:
                    if (cur_char == '\r') {
                        state = State::StatusMsgEnd_rnr;
                    } else if (isalnum(cur_char)) {
                        prev_it = it;
                        state = State::HeaderName;
                    }
                    break;
        
                case State::StatusMsgEnd_rnr:
                    if (cur_char == '\n') {
                        return ResponseParseResult::Completed;
                    } else
                        return ResponseParseResult::Error;

                case State::HeaderName:
                    if (cur_char == ':') {
                        header_name = std::string_view{prev_it, it};
                        state = State::HeaderNameEnd;
                    } else if (is_http_control(cur_char)) {
                        return ResponseParseResult::Error;
                    }
                    break;

                case State::HeaderNameEnd:
                    if (cur_char == ' ') {
                        state = State::HeaderNameValueSpace;
                    } else if (!is_http_control(cur_char)) {
                        state = State::HeaderValueStart;
                        prev_it = it;
                    } else
                        return ResponseParseResult::Error;
                    break;

                case State::HeaderNameValueSpace:
                    if (!is_http_control(cur_char)) {
                        state = State::HeaderValueStart;
                        prev_it = it;
                    } else
                        return ResponseParseResult::Error;
                    break;

                case State::HeaderValueStart:
                    if (!is_http_control(cur_char)) {
                        continue;
                    } else if (cur_char == '\r') {
                        res.headers.emplace_back(
                            Response::Header{header_name, std::string_view{prev_it, it}}
                        );
                        if (header_name == "Transfer-Encoding" && std::string_view{prev_it, it} == "chunked") {
                            res.is_chunked = true;
                        } else if (header_name == "Content-Length") {
                            content_length = atoi(std::string_view{prev_it, it}.data());
                        }
                        state = State::Header_r;
                    } else 
                        return ResponseParseResult::Error;
                    break;

                case State::Header_r:
                    if (cur_char == '\n') {
                        state = State::Header_rn;
                    } else 
                        return ResponseParseResult::Error;
                    break;

                case State::Header_rn:
                    if (cur_char == '\r') {
                        state = State::Header_rnr;
                    } else if (!is_http_control(cur_char)) {
                        prev_it = it;
                        state = State::HeaderName;
                    } else 
                        return ResponseParseResult::Error;
                    break;

                case State::Header_rnr:
                    if (cur_char == '\n') {
                        if (res.is_chunked)
                            state = State::NewChunkStart;
                        else {
                            state = State::DataStart;
                        }
                    } else {
                        return ResponseParseResult::Error;
                    }  
                    break;

                case State::DataStart:
                    res.content = std::string_view{it, it+content_length};
                    return ResponseParseResult::Completed;
                
                case State::NewChunkStart:
                    if (is_hex_char(cur_char)) {
                        prev_it = it;
                        state = State::ChunkDataSize;
                    } else 
                        return ResponseParseResult::Error;
                    break;

                case State::ChunkDataSize:
                    if (is_hex_char(cur_char)) {
                        continue;
                    } else if (cur_char == '\r') {
                        // chunk size is hex
                        new_chunk_size = std::stoi(std::string_view{prev_it, it}.data(), 0, 16);
                        if (new_chunk_size)
                            state = State::ChunkDataSize_r;
                        else
                            state = State::ZeroChunkSize_r;
                    } else 
                        return ResponseParseResult::Error;
                    break;

                case State::ZeroChunkSize_r:
                    if (cur_char == '\n') {
                        return ResponseParseResult::Completed;
                    } else
                        return ResponseParseResult::Error;

                case State::ChunkDataSize_r:
                    if (cur_char == '\n') {
                        state = State::ChunkDataSize_rn;
                    } else
                        return ResponseParseResult::Error;
                    break;

                case State::ChunkDataSize_rn:
                    if (std::end(buf) - it > new_chunk_size) {
                        res.chunks.emplace_back(
                            Response::Chunk{new_chunk_size, std::string_view{it, it+new_chunk_size}}
                        );
                        it += new_chunk_size - 1;
                        std::cout << "chunk size: " << new_chunk_size << std::endl;
                        state = State::ChunkDataEnd;
                    } else {
                        return ResponseParseResult::Error;
                    }
                    break;

                case State::ChunkDataEnd:
                    if (cur_char == '\r') {
                        state = State::ChunkData_r;
                    } else 
                        return ResponseParseResult::Error;
                    break;
                
                case State::ChunkData_r:
                    if (cur_char == '\n') {
                        state = State::NewChunkStart;
                    } else {
                        return ResponseParseResult::Error;
                    }
                    break;

                default:
                    return ResponseParseResult::Error;
            }

        }
        return ResponseParseResult::InCompleted;
    }

    static bool is_all_digit(std::string_view data) {
        for (auto ch: data) {
            if (!isdigit(ch)) return false;
        }
        return true;
    }

    static bool is_http_control(char ch) {
        return ch <= 31 || ch == 127;
    }

    static bool is_hex_char(char ch) {
        if (ch >= '0' && ch <= '9') return true;
        else if (ch >= 'a' && ch <= 'f') return true;
        else if (ch >= 'A' && ch <= 'F') return true;
        return false;
    }
    State state;
};

}

} // namespace sheep