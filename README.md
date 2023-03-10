# 简介

实现了基于C++20协程和异步IO的网络框架，可快速编写`HTTP`服务器，相比使用`asio`等异步框架代码可读性更强;
使用`io_uring`实现异步IO，性能优于`epoll`；基于无锁数据结构实现了异步日志模块，可单独使用；
采用`header-only`的方式，直接`include`即可开搞！


## 实现细节

1. 实现了基于C++20协程和异步IO的网络框架，可快速编写TCP服务器；
2. 基于逻辑过程用串行的方式实现业务代码，可读性更强，避免回调地狱；
3. 基于无锁数据结构实现了异步日志模块，使用fmtlib实现格式化写入；
4. 使用有限状态机解析HTTP协议，使用string_view实现内存的零拷贝；
5. 将io_uring封装为io_service，解决了io_uring本身线程不安全的问题；
6. 使用线程池调度协程，充分发挥多核处理器的性能；
7. 使用无锁数据结构实现协程队列，空闲线程自动获取协程，简单高效，无需实现额外的调度算法。

## 编译和运行

### 依赖

1. 使用GCC 12.2.1 开发（用到了C++20标准）;
2. Linux >= 5.6，建议 >= 5.11，必须要支持io_uring;
    - 运行 `uname -r` 即可查看你的内核版本。
3. [fmtlib](https://github.com/fmtlib/fmt);
    - 定义`FMT_HEADER_ONLY`，不需要链接。
4. [liburing](https://github.com/axboe/liburing).

### 编译命令

*单元测试还没写完，目前上传的代码不包括测试文件。*
```bash
xmake config -m debug 
# xmake config -m release
xmake
```

## 代码示例


### echo_server

快速实现一个简单的 [echo_server](./examples/echo_server.cpp)

```cpp
#define FMT_HEADER_ONLY

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
    co_return;
}

int main(int argc, char* argv[]) {
    // 创建监听地址: localhost:9090
    auto addr = net::make_loopback_v4(9090);
    
    // 创建TCP Server, 设置线程数：4
    Server echo_server(addr, 4);

    // 设置连接处理函数
    echo_server.set_handler(session);

    // 等待echo_server结束
    sync_wait(echo_server.serve());

    return 0;
}
```

服务端输出：

```
Server listen on: 127.0.0.1:9090
 accept client: 127.0.0.1:56298
2023-03-10 03:54:10.157 DEBUG 5572130208773741465 [sheep::task<void> session(std::unique_ptr<sheep::net::Connection>):echo_server.cpp@24] client:127.0.0.1:56298 connected.
2023-03-10 03:54:11.875 DEBUG 5572130208773741465 [sheep::task<void> session(std::unique_ptr<sheep::net::Connection>):echo_server.cpp@31] 127.0.0.1:56298: <6 bytes read> hello

2023-03-10 03:54:11.875 DEBUG 5572130208773741465 [sheep::task<void> session(std::unique_ptr<sheep::net::Connection>):echo_server.cpp@39] close connection: 127.0.0.1:56298
```

客户端：

```bash
% nc 127.0.0.1 9090
hello
hello
```

*TODO: more examples.*

## TODO

1. 添加单元测试；
2. 优化日志模块的接口；
3. 添加更多用例；
4. 性能测试。