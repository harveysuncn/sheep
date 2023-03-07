#pragma once

#include <arpa/inet.h>
#include <cstdint>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <cstring> // memset

namespace sheep {

namespace net {


enum class Protocol
{
    Ipv4, Ipv6
};


class Address
{
public:
    /*
    A std::string_view doesn't provide a conversion to a const char* because it doesn't store a null-terminated string. 
    See https://stackoverflow.com/questions/48081436/how-you-convert-a-stdstring-view-to-a-const-char
    */
    static constexpr std::string_view any_ipv4{"0.0.0.0\0"};
    static constexpr std::string_view any_ipv6{"::\0"};
    static constexpr std::string_view loopback_ipv4{"127.0.0.1\0"};
    static constexpr std::string_view loopback_ipv6{"::1\0"};

    Address() noexcept {
        std::memset(&addr_, 0, sizeof(addr_));
        addr_len_ = sizeof(addr_);
    }

    Address(const char* ip_addr, uint16_t port, Protocol version = Protocol::Ipv4)
        : protocol_(version)
    {
        std::memset(&addr_, 0, sizeof(addr_));
        addr_len_ = sizeof(addr_);
        if (protocol_ == Protocol::Ipv4) {
            auto ipv4_addr = reinterpret_cast<struct sockaddr_in*>(&addr_);
            ipv4_addr->sin_family = AF_INET;
            inet_pton(AF_INET, ip_addr, &ipv4_addr->sin_addr.s_addr);
            ipv4_addr->sin_port = htons(port);
        } else {
            auto ipv6_addr = reinterpret_cast<struct sockaddr_in6*>(&addr_);
            ipv6_addr->sin6_family = AF_INET6;
            inet_pton(AF_INET6, ip_addr, &ipv6_addr->sin6_addr);
            ipv6_addr->sin6_port = htons(port);
        }
    }

    Address(const Address&) = default;

    Protocol protocol() const noexcept { return protocol_; }

    struct sockaddr* sockaddr() { return &addr_; }

    uint16_t port() const noexcept {
        if (protocol_ == Protocol::Ipv4) {
            auto addr = reinterpret_cast<const struct sockaddr_in*>(&addr_);
            return ntohs(addr->sin_port);
        } else {
            auto addr = reinterpret_cast<const struct sockaddr_in6*>(&addr_);
            return ntohs(addr->sin6_port);
        }
    }

    std::string ip_address() const noexcept {
        if (protocol_ == Protocol::Ipv4) {
            auto addr = reinterpret_cast<const struct sockaddr_in*>(&addr_);
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr->sin_addr, ip, INET_ADDRSTRLEN);
            return ip;
        } else {
            auto addr = reinterpret_cast<const struct sockaddr_in6*>(&addr_);
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET6, &addr->sin6_addr, ip, INET6_ADDRSTRLEN);
            return ip;
        }
    }

    socklen_t* len() { return &addr_len_; }

    std::string to_string() const noexcept {
        return ip_address() + ":" + std::to_string(port());
    }

    friend std::ostream& operator<<(std::ostream& os, const Address& addr) {
        os << addr.to_string();
        return os;
    }

private:
    Protocol protocol_;
    struct sockaddr addr_;
    socklen_t addr_len_;
};


inline Address make_any_address_v4(uint16_t port) {
    return Address{Address::any_ipv4.data(), port, Protocol::Ipv4};
}

inline Address make_any_address_v6(uint16_t port) {
    return Address{Address::any_ipv6.data(), port, Protocol::Ipv6};
}

inline Address make_loopback_v4(uint16_t port) {
    return Address{Address::loopback_ipv4.data(), port, Protocol::Ipv4};
}

inline Address make_loopback_v6(uint16_t port) {
    return Address{Address::loopback_ipv6.data(), port, Protocol::Ipv6};
}

} // namespace net

} // namespace sheep