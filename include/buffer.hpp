#pragma once 

#include <cstdlib>
#include <cstring>
#include <string_view>

namespace sheep {


class Buffer
{
public:
    static constexpr std::size_t kDEFAULT_BUFFER_CAPACITY = 1024;
    explicit Buffer(std::size_t capacity = kDEFAULT_BUFFER_CAPACITY)
        : size_(0)
        , capacity_(capacity)
    {
        buf_ = static_cast<std::byte*>(std::malloc(sizeof(std::byte) * capacity_));
    }

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    ~Buffer() {
        std::free(buf_);
    }

    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return capacity_; }
    void set_size(std::size_t written) noexcept { size_ = written; }

    void clear() { if (size_) std::memset(buf_, 0, sizeof(std::byte) * size_); }
    const unsigned char* data() const noexcept { return reinterpret_cast<const unsigned char*>(buf_); }

    void write(const unsigned char* data, std::size_t write_size) {
        clear();
        size_ = write_size;
        std::memcpy(buf_, data, size_);
    }

    std::string_view to_string() const noexcept {
        return {reinterpret_cast<const char*>(buf_), size_};
    }

private:
    std::byte* buf_;
    std::size_t size_;
    std::size_t capacity_;
};


}