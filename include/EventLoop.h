#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include <mutex>
#include <sys/epoll.h>
#include <sys/types.h>

#include "Timer.h"
#include "others.h"
#include "Httpdata.h"

// 类前置声明
class HttpData;
class TimeWheel;
class Chanel;

class EventLoop{

private:
    bool isMainReactor;                         // 是否是主Reactor线程
    int Conn_num;                               // 该Reactor管理的http连接数量
    std::mutex lock;                            // 互斥锁，保护共有数据

    static const int MaxEvent = 4096;           // 线程最大关联事件数量
    static const int Epoll_timeout = 100000;    // 超时时间，单位是ms

    int epollfd;                                // epoll文件描述符
    epoll_event events[MaxEvent];               // epoll事件数组

    Chanel* chanelpool[GlobalValue::Maxconn];   // 这个是Chanel的数组，一个连接对应一个Chanel
    
    // 现在该成员分配给各个Chanel了
    // HttpData* http_data;

    TimeWheel* time_wheel;

    bool stop;

public:
    explicit EventLoop(bool ismain);
    ~EventLoop();
    
    // 对epoll进行加减修改操作
    bool AddChanel(Chanel* CHNL);
    bool ModChanel(Chanel* CHNL, __uint32_t EV);
    bool DelChanel(Chanel* CHNL);
    
    int Get_Conn_Num();

    void StartLoop();
    void StopLoop();

    int Get_epollfd()   {return epollfd;}

    TimeWheel* Get_TimeWheel()  {return time_wheel;}

private:
    void ListenNCall();

};


#endif