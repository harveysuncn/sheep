#pragma once

#include <memory>
#include <coroutine>

#include "buffer.hpp"
#include "io_service.hpp"
#include "net/socket.hpp"

namespace sheep {


namespace net {


class Connection
{
public:
    explicit Connection(std::unique_ptr<Socket> conn_socket)
        : sock_(std::move(conn_socket))
        , read_buf_(std::make_unique<Buffer>())
        , write_buf_(std::make_unique<Buffer>())
    {

    }

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    Connection(Connection&& other) noexcept 
        : sock_(std::move(other.sock_))
        , read_buf_(std::move(other.read_buf_))
        , write_buf_(std::move(other.write_buf_))
        , ios_(other.ios_)
    {}

    ~Connection() noexcept {
        ::close(get_fd());
    }

    void set_client_addr(const net::Address& addr) noexcept {
        addr_ = addr;
    }
    const net::Address& client_addr() const noexcept { return addr_; }

    int get_fd() const noexcept { return sock_->fd(); }

    Socket* get_socket() noexcept { return sock_.get(); }

    Buffer* read_buf() noexcept { return read_buf_.get(); }

    Buffer* write_buf() noexcept { return write_buf_.get(); }

    void set_io_service(io_service* ios) noexcept { ios_ = ios; }
    io_service* get_io_service() noexcept { return ios_; }

    task<int> recv() {
        assert(ios_ != nullptr);
        read_buf_->clear();
        int bytes_read = co_await ios_->recv(get_fd(), (void*)read_buf_->data(), read_buf_->capacity(), 0);
        read_buf_->set_size(bytes_read);
        co_return bytes_read;
    }

    task<int> send() {
        assert(ios_ != nullptr);
        int bytes_sent = co_await
            ios_->send(
                get_fd(), (void*)write_buf_->data(), write_buf_->size(), 0);

        co_return bytes_sent;
    }


private:
    std::unique_ptr<Socket> sock_;
    net::Address addr_;
    std::unique_ptr<Buffer> read_buf_;
    std::unique_ptr<Buffer> write_buf_;
    io_service* ios_{nullptr};
};

} // namespace net

} // namespace sheep