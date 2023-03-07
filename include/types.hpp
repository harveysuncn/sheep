#pragma once

#include <coroutine>
#include <atomic>

#include "net/connection.hpp"

namespace sheep {


struct thread_meta
{

    std::uint16_t thread_id;

};


struct session_wrapper
{
    std::coroutine_handle<> coro;
    net::Connection* conn;
};

}