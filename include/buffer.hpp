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

    Buffer(Buffer&& other) noexcept 
        : buf_(other.buf_), size_(other.size_), capacity_(other.capacity_)
    {
        other.buf_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    Buffer& operator=(Buffer&& other) noexcept
    {
        if (this == &other) return *this;
        std::swap(buf_, other.buf_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
        return *this;
    }

    friend void swap(Buffer&, Buffer&) noexcept;
    ~Buffer() {
        std::free(buf_); // its safe to free nullptr
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

inline void swap(Buffer& a, Buffer& b) noexcept {
    using std::swap;
    swap(a.buf_, b.buf_);
    swap(a.size_, b.size_);
    swap(a.capacity_, b.capacity_);
}

}