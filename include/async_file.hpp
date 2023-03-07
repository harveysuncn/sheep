#pragma once

#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <span>
#include <filesystem>
#include <sys/stat.h>

#include "io_service.hpp"

namespace sheep {


enum class file_option {
    ReadOnly,
    Truncate,
    Append,
    ReadWrite,
    RWTruncate,
    RWAppend
};


static constexpr auto open_options(file_option option)
{
    switch (option)
    {
        case file_option::ReadOnly:
            return O_RDONLY;
        
        case file_option::Truncate:
            return O_WRONLY | O_CREAT | O_TRUNC;

        case file_option::Append:
            return O_WRONLY | O_CREAT | O_APPEND;

        case file_option::ReadWrite:
            return O_RDWR;

        case file_option::RWTruncate:
            return O_RDWR | O_CREAT | O_TRUNC;
        
        case file_option::RWAppend:
            return O_RDWR | O_CREAT | O_APPEND;
    }

    return 0;
}

static constexpr mode_t kDefaultFileMode = S_IRUSR | S_IWUSR;

class async_file
{
public:
    async_file(std::filesystem::path path, io_service& ios, 
        file_option options = file_option::ReadOnly, mode_t file_mode = kDefaultFileMode)
        : file_path_(path)
        , ios_(ios)
        , file_mode_(file_mode)
        , opt_flags(options)
    {
    }


    ~async_file() {
        if (fd_ > 0)
            close(fd_);
        std::cout << "file closed" << std::endl;
    }

    task<int> open() {
        fd_ = co_await open_impl();
        co_return fd_;
    }

    task<uint64_t> size() {
        co_await file_statx();
        auto size = file_statx_->stx_size;
        co_return size;
    }

    /// read file to buf
    /// \return bytes read
    task<int> read(std::span<std::byte> buf, uint64_t size, uint64_t offset = 0) {
        auto ret = co_await ios_.read(fd_, buf.data(), size, offset);
        co_return ret;
    }

    /// write buf to file
    /// \return bytes writen
    task<int> write(std::span<std::byte> buf) {
        auto ret = co_await ios_.write(fd_, buf.data(), buf.size(), 0);
        co_return ret;
    }

private:
    io_awaitable open_impl() {
        return ios_
        .openat(AT_FDCWD, file_path_.c_str(), open_options(opt_flags), file_mode_)
        ;
    }

    io_awaitable close_impl() {
        return ios_.close(fd_);
    }

    task<void> file_statx() {
        if (file_statx_) co_return;
        file_statx_ = std::make_unique<struct statx>();
        std::memset(file_statx_.get(), 0, sizeof(struct statx));
        co_await ios_.statx(AT_FDCWD, file_path_.c_str(), open_options(opt_flags), STATX_ALL, file_statx_.get());
        co_return;
    }

private:
    std::filesystem::path file_path_;
    io_service& ios_;
    mode_t file_mode_;
    file_option opt_flags;
    int fd_{0};
    std::unique_ptr<struct statx> file_statx_;
};



} // namespace sheep