## 烂大街的WEBSERVER项目

### Intorduction

​	一个基于C++17的WEBSERVER服务器。支持GET、POST与HEAD方法。实现HTTP的长连接与段连接。日志使用[spdlog](https://github.com/gabime/spdlog)。

### Environment

- OS: Ubuntu 20.04
- Compiler: g++ 9.4.0
- CMake:  3.16.3

### Build

```shell
mkdir build
cd build
cmake ..
make
```

### Usage

```shell
./WEBSERVER -p  port_number  -s  subreactor_number  -l  log_file_path(start with ./）
```

### Technical points

- 使用边沿触发的Epoll多路复用技术,并使用多Reactor模型。
- 使用线程池避免线程频繁创建销毁的开销
- 基于时间轮算法的定时器以实现请求报文传输以及长连接的超时回调。
- One loop per thread，主线程MainReactor负责accept并将连接socket分发给SubReactors；子线程中的SubReactors负责监听连接socket上的事件以及调用相应的回调函数。
- 使用状态机和正则表达式解析HTTP请求，支持管线化。