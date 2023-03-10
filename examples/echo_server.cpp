#define FMT_HEADER_ONLY
#include <memory>
#include <iostream>

#include "log/logger.hpp"
#include "net/address.hpp"
#include "server.hpp"
#include "task.hpp"
#include "sync_wait.hpp"
#include "net/connection.hpp"
#include "log/log.hpp"
using namespace sheep;

log::LoggerImpl<4, 1024> logger;

task<> session(std::unique_ptr<net::Connection> conn) {
    // 获取客户端信息
    auto client_sock = conn->get_socket();
    auto client_addr = conn->client_addr();

    // 获取当前协程的io service
    auto ios = conn->get_io_service();

    logger.debug("client:{} connected.", client_addr.to_string());

    int bytes = co_await conn->recv();
    if (bytes < 1) {
        logger.debug("{}: <zero bytes read, exit>", client_addr.to_string());
        co_return;
    } else {
        logger.debug("{}: <{} bytes read> {}", client_addr.to_string(), bytes, conn->read_buf()->to_string());
    }

    // 可以在两个buffer之间复制数据
    // conn->write_buf()->write(conn->read_buf()->data(), bytes);
    // 或者直接交换两个buffer
    swap(*conn->write_buf(), *conn->read_buf());
    co_await conn->send();
    logger.debug("close connection: {}", client_addr.to_string());
}

int main(int argc, char* argv[]) {
    // 创建监听地址: localhost:9090
    auto addr = net::make_loopback_v4(9090);
    
    // 创建TCP Server, 设置线程数：4
    Server echo_server(addr, 4);

    // 设置连接处理函数
    echo_server.set_handler(session);

    // 同步等待echo_server
    sync_wait(echo_server.serve());

    return 0;
}