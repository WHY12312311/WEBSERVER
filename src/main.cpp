#include <tuple>
#include <optional>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <signal.h>
#include <sys/types.h>

#include "ThreadPool.h"
#include "others.h"
#include "EventLoop.h"
#include "Server.h"
#include "Timer.h"

// 写成全局的主要是方便信号处理函数来调用
static SERVER* service;
static ThreadPool* thread_pool;

// 关闭服务器
void shutdown(int num){
    Getlogger()->info("Sever shutting down by signal {}.....", strsignal(num));
    // 关闭服务器
    service->Stop_server();
    // 关闭线程池
    thread_pool->Stop_thread();
    delete thread_pool;
    // 关闭日志
    Getlogger()->flush();
    // Getlogger()->~logger();
    std::cout << "Server stopped....." << std::endl;
    // 进程退出
    exit(0);
}

int main(int argc, char ** argv){

    // 接受并解析命令行
    auto res = GetCommand(argc, argv);
    if (res == std::nullopt){
        std::cout << "Wrong args! Please insert as: WEBSERVER -p portnumber -s ReactorSize -l logPath." << std::endl;
        return 0;
    }

    // 注册信号处理函数
    Sig_register(SIGPIPE, SIG_IGN);
    Sig_register(SIGQUIT, shutdown);

    // 开启日志
    if (!Getlogger(std::get<2>(*res))){
        std::cout << "Failed to initialize the logger!" << std::endl;
        return 0;
    }
    Getlogger()->set_pattern("[%H:%M:%S %z] [%n] [%^---%L---%$] [thread %t] %v");   // 设置日志的形式
    Getlogger()->info("A brand new server started......................................");

    // 创建线程池
    thread_pool = new ThreadPool(std::get<1>(*res));
    
    // 创建main reactor
    EventLoop mainReactor = EventLoop(true);

    // 创建单例模式服务器
    std::cout << "Server started. To stop this server, use ctrl+\\" << std::endl;
    service = SERVER::server_init(std::get<0>(*res), std::get<1>(*res), thread_pool, &mainReactor);

    // 创建子线程来监听时间
    std::vector<TimeWheel*> time_pool = SERVER::time_pool;
    std::thread time_thread([time_pool] {Run_Timer(time_pool);});

    service->Start_server();

    return 0;
}