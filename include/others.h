#ifndef OTHER_H
#define OTHER_H

#include <optional>
#include <tuple>
#include <string>
#include <unistd.h>
#include <regex>
#include <iostream>
#include <chrono>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <signal.h>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

// 定义一些全局变量
class GlobalValue{

public:
    static int SlotTime;                            // 时间轮一个槽的时间
    static const int Maxconn = 100000;              // 最大连接数
    static std::chrono::seconds conn_timeout;       // 连接的超时时间
    static std::chrono::seconds post_timeout;       // 连接的超时时间
    static int ListenQueue;                         // listen函数中最大队列数
    static int readbuf_size;                        // httpdata读缓冲区大小
    static int writebuf_size;                       // httpdata写缓冲区大小
};

// 解析命令行语句，生成结果
std::optional<std::tuple<int, int, std::string>> GetCommand(int argc, char ** argv);

// 设置文件描述符非阻塞
void SetNonBlocking(int fd);


// 封装的写函数
int Write_data(int fd, std::string& content, bool& isfull);


// 封装的读函数
int Read_data(int fd, std::string& read_buf, bool& is_conn);


// socket连接
int bindNlisten(__uint32_t portnum);


// 获取事件
std::string GetTime();

// 辅助注册信号处理函数
void Sig_register(int signum, void(handler)(int));

// 生成唯一的logger对象，这个地方给一个默认参数是为了后面不带参数的运行
std::shared_ptr<spdlog::logger> Getlogger(std::string path = "./log/log.txt"); 

#endif