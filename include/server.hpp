#pragma once

#include <coroutine>
#include <memory>
#include <thread>
#include <functional>
#include <iostream>

#include "task.hpp"
#include "types.hpp"
#include "io_service_pool.hpp"
#include "net/address.hpp"
#include "net/socket.hpp"
#include "net/connection.hpp"
#include "thread_pool.hpp"

namespace sheep {



class Server
{
public:
    using handler_t = sheep::task<> (*)(std::unique_ptr<sheep::net::Connection>);

    explicit Server(net::Address listen_addr, int concurrency = std::thread::hardware_concurrency())
        : listen_addr_(listen_addr)
        , io_services_(concurrency)
        , thread_pool_(concurrency, io_services_)
    {
        listen_sock_.bind(listen_addr_, true);
        listen_sock_.listen();
    }

    void set_handler(handler_t h) {
        client_handler_ = h;
    }

    task<> serve() {
        assert(client_handler_ != nullptr);
        thread_pool_.start();
        // log
        std::cout << "Server listen on: " << listen_addr_.to_string() << std::endl;
        while (true)
        {
            // accept new connection
            net::Address client_addr;

            auto client_fd = listen_sock_.accept(client_addr);
            if (client_fd < 1) continue;
            // log
            std::cout << " accept client: " << client_addr.to_string() << std::endl;
            auto client_sock = std::make_unique<net::Socket>(client_fd);
            auto conn = std::make_unique<net::Connection>(std::move(client_sock));
            conn->set_client_addr(client_addr);
            auto pconn = conn.get();
            auto session = client_handler_(std::move(conn));

            thread_pool_.submit(session_wrapper{session.detach(), pconn});
        }
        co_return;
    }

private:
    net::Address listen_addr_;
    net::Socket listen_sock_;
    io_service_pool io_services_;
    thread_pool thread_pool_;
    std::function<sheep::task<void>(std::unique_ptr<net::Connection>)> client_handler_{nullptr};
};


}