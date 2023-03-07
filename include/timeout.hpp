#pragma once

#include <chrono>
#include <linux/time_types.h>

#include "io_service.hpp"

namespace sheep
{

template <typename Rep, typename Period>
constexpr __kernel_timespec duration_to_timespec(std::chrono::duration<Rep, Period> duration) {
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    duration -= seconds;
    auto nanosecs = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
    return __kernel_timespec{seconds.count(), nanosecs.count()};
}

struct timeout_duration
{
    template <typename Rep, typename Period>
    timeout_duration(std::chrono::duration<Rep, Period> duration, io_service* s)
        : ts(duration_to_timespec(duration))
        , ios(s)
    {}

    io_awaitable operator()() {
        return ios->timeout(&ts);
    }

    io_service* ios;
    struct __kernel_timespec ts;

};




}