#ifndef CHANEL_H
#define CHANEL_H

#include <iostream>
#include <unistd.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <functional>
#include <fcntl.h>
#include "Timer.h"
#include "EventLoop.h"
#include "Httpdata.h"

class Timer;
class EventLoop;
class HttpData;

class Chanel {

private:
    int epollfd;                    // 归属于哪个epollfd
    int fd;                         // 关联的文件描述符
    bool is_connect;                // 是否连接
    bool is_events;                 // 是否已经设置evnets
    __uint32_t events;              // 要监视的事件集
    __uint32_t revents;             // 已经就绪的事件集

    Timer* timer = nullptr;         // 计时器，用于在时间轮中检测超时

    EventLoop* eventloop;           // 该Chanel从属的reactor

    HttpData* httpdata;             // 该Chanel对应的httpdata

    // 下面使用各种handle来处理事件
    using CALLBACK = std::function<void()>;
    CALLBACK read_handle;
    CALLBACK write_handle;
    CALLBACK error_handle;
    CALLBACK disconn_handle;

public:

    // 已经迁移到了HttpData类中
    // char read_buf[readbuf_size];    // 读缓冲区
    // char write_buf[writebuf_size];  // 写缓冲区
    // int bytes_read;                 // 读取数量
    // int bytes_write;                // 写入数量

    enum handler_enum{
        H_READ = 0, H_WRITE, H_ERROR, H_DISCONN
    };

    explicit Chanel(int _fd, int _epollfd, bool _is_conn, EventLoop* _eventloop);
    ~Chanel();

    // 注册处理函数
    void HandlerRegister(handler_enum Hnum, CALLBACK fun);

    // 关于成员变量的设置读取接口
    __uint32_t Get_events() {return events;}
    __uint32_t Get_revents() {return revents;}
    EventLoop* Get_loop()  {return eventloop;}
    HttpData* Get_http()    {return httpdata;}

    // 重置EPOLLONESHOT
    void Set_oneshot(){
        events  = events | EPOLLONESHOT;
        Set_Epoll();
    }

    // 修改监听事件集
    void Set_events(__uint32_t newe) {
        events = newe;
        Set_Epoll();
    }

    void Set_Epoll(){
        // 如果该文件描述符已经被关闭了，则不进行处理
        // 由于在处理epoll请求的时候有删除epoll这一条
        // 而删除之后还是在这个数组中，可能会导致bug
        if (fcntl(fd, F_GETFD) == -1)
            return;
        epoll_event ev;
        ev.data.fd = fd;
        ev.events = events;
        int ret = 0;
        if (!is_events){
            is_events = true;
            ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
        }
        else 
            ret = epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
        if (ret < 0){
            perror("epoll_ctl");
            return;
        }
    }

    void Set_events_out(bool isout);

    void Set_revents(__uint32_t newr)   {revents = newr;}
    void Set_loop(EventLoop* newe)    {eventloop = newe;}

    int Get_fd()        {return fd;}
    bool Get_isconn()   {return is_connect;}
    
    void Set_timer(Timer* newtimer)     {timer = newtimer;}
    Timer* Get_timer()                  {return timer;}

    // 执行handler
    void CallHandler(handler_enum Hnum);

    // 分析执行revents
    void Callrevents();
};

#endif