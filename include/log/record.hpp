#pragma once

#include <chrono>
#include <cstddef>
#include <iterator>
#include <thread>
#include <iostream>
#include <cstring>

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/chrono.h>

#include "log/loglevel.hpp"


namespace sheep {

namespace log {


using Time_t = std::chrono::time_point<std::chrono::system_clock>;

template <std::size_t RecordSize>
struct FixedSizeRecord
{
    static constexpr std::size_t Size = RecordSize;

    FixedSizeRecord() noexcept { clear(); }

    ~FixedSizeRecord() noexcept { clear(); }

    FixedSizeRecord(const FixedSizeRecord& rhs) noexcept {
        memcpy(this->data, rhs.data, RecordSize);
        size = rhs.size;
    }

    FixedSizeRecord(FixedSizeRecord&& rhs) noexcept {
        memcpy(this->data, rhs.data, RecordSize);
        size = rhs.size;
    }

    char data[RecordSize];
    std::size_t size{0};
private:
    void clear() { memset(data, 0, RecordSize); size = 0; }
};


template <typename T>
struct MakeRecordImpl
{
    template <typename S, typename... Args>
    void operator()(T* out, S format, Args&&... args)
    {
        Time_t nowTime(std::chrono::system_clock::now());
        auto nowMs = std::chrono::floor<std::chrono::milliseconds>(nowTime.time_since_epoch());

        auto res = fmt::format_to_n(out->data, T::Size, format, nowTime, nowMs, std::forward<Args>(args)...);
        out->size = res.size;
    }
};

static constexpr std::size_t DesireRecordSize = 128;
using Record = FixedSizeRecord<DesireRecordSize>;
using MakeRecord = MakeRecordImpl<Record>;

} // namespace log

} // namespace sheep