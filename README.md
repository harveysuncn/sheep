# 简介

实现了基于C++20协程和异步IO的网络框架，可快速编写`HTTP`服务器，相比使用`asio`等异步框架代码可读性更强;
使用`io_uring`实现异步IO，性能优于`epoll`；基于无锁数据结构实现了异步日志模块，可单独使用；
采用`header-only`的方式，直接`include`即可开搞！


## 实现细节

1. 基于Coroutine-TS 实现了`task`, `sync_wait`, `event`等结构；
2. 日志模块前端使用fmtlib实现日志的格式化，支持用户自定义日志格式；
3. 日志模块后端使用缓冲区实现异步写入文件，减少IO操作；
4. 使用有限状态机解析HTTP协议，使用`string_view`实现内存的零拷贝；
5. 将io_uring封装为`io_service`，解决了io_uring本身线程不安全的问题；
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
xmake
```

## 代码示例

**TODO**

## TODO

1. 添加单元测试；
2. 优化日志模块的接口；
3. 添加更多用例；
4. 性能测试。