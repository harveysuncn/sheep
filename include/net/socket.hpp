#pragma once 

#include <asm-generic/socket.h>
#include <stdexcept>
#include <utility>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>
#include <iostream>

#include "address.hpp"

namespace sheep {

namespace net {

class Socket
{
public:
    static constexpr int kBACK_LOG = 128;

    Socket() = default;

    explicit Socket(int fd) : fd_(fd) {}

    ~Socket() {
        if (fd_ != -1) ::close(fd_);
        fd_ = -1;
    }

    Socket(Socket&& other) noexcept {
        fd_ = std::exchange(other.fd_, -1);
    }

    Socket& operator=(Socket&& rhs) noexcept {
        if (fd_ != -1) ::close(fd_);
        fd_ = std::exchange(rhs.fd_, -1);
        return *this;
    }

    int fd() const noexcept { return fd_; }

    void bind(Address& serve_addr, bool resuable = true) {
        if (fd_ == -1) {
            create_socket(serve_addr.protocol());
        }
        if (resuable)
            set_reusable();
        if (::bind(fd_, serve_addr.sockaddr(), *serve_addr.len()) == -1) {
            std::cerr << strerror(errno) << std::endl;
            throw std::logic_error("Socket: bind() error!");
        }
    }

    void connect(Address& addr) {
        if (fd_ == -1) {
            create_socket(addr.protocol());
        }

        if (::connect(fd_, addr.sockaddr(), *addr.len()) == -1)
            throw std::logic_error("Socket: connect() error!");
    }

    void listen() {
        assert(fd_ != -1);
        if (::listen(fd_, kBACK_LOG) == -1)
            throw std::logic_error("Socket: listen() error!");
    }

    int accept(Address& addr) {
        assert(fd_ != -1);
        int client_fd = -1;
        if ((client_fd = ::accept(fd_, addr.sockaddr(), addr.len())) == -1) [[unlikely]]
        {
            // unlikely
            throw std::logic_error("Socket: accept() error!");
        }
        return client_fd;
    }

    void set_reusable() {
        assert(fd_ != -1);

        int ok = 1;
        if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &ok, sizeof(ok)) == -1
            || ::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &ok, sizeof(ok)) == -1)
        {
            std::cerr << strerror(errno) << std::endl;
            throw std::logic_error("Socket: set_reusable() error!");
        }
    }

    void set_nonblocking() {
        assert(fd_ != -1);

        if (::fcntl(fd_, F_SETFL, ::fcntl(fd_, F_GETFL) | O_NONBLOCK) == -1)
            throw std::logic_error("Socket: set_nonblocking() error!");
    }

    int get_attrs() {
        assert(fd_ != -1);
        return ::fcntl(fd_, F_GETFL);
    }

private:
    void create_socket(Protocol p) {
        if (p == Protocol::Ipv4)
            fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        else
            fd_ = ::socket(AF_INET6, SOCK_STREAM, 0);
        if (fd_ == -1)
            throw std::logic_error("Socket: create socket failed!");
    }

    int fd_{-1};

};


}


}