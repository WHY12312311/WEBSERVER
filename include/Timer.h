#ifndef TIMER_H
#define TIMER_H

#include <functional>
#include <vector>
#include <list>
#include <chrono>
#include <signal.h>
#include "Httpdata.h"
#include "others.h"
#include "Chanel.h"


// 前置声明
class HttpData;
class Chanel;

// 2023.3.13：Timer类并没有内置计时器，他只记住了在槽中的位置和圈数。主要是用作关联回调函数的。
class Timer{

private:
    using CALLBACK = std::function<void()>; // CALLBACK定义为一个没有参数，返回为void的函数
    CALLBACK FuncOfTimeUp;                  // 超时回调函数
    size_t PosInWheel;                      // 位于时间轮中的哪个槽
    size_t Turns;                           // 当前剩下多少圈
    HttpData* time_http;                    // 与其耦合的httpdata

public:
    // 构造函数，并设定超时回调函数为解除连接
    Timer(size_t pos, size_t turns, HttpData* _http);

    size_t Timer_GetPos()                   {return PosInWheel;}
    void Timer_SetPos(size_t new_pos)       {PosInWheel = new_pos;}

    size_t Timer_GetTurns()                 {return Turns;}
    void Timer_SetTurns(size_t new_turns)   {Turns = new_turns;}

    void Timer_TurnsDecline()               {Turns--;}

    friend class TimeWheel;

    void Timer_SetCall(CALLBACK Fun)        {FuncOfTimeUp = Fun;}

private:
    void DoCallback();      // 定义成私有保证只有友元函数才能够调用使用
};




class TimeWheel{
private:
    std::vector<std::list<Timer*>> slot;        // 时间轮的槽
    size_t SizeOfWheel;                         // 每一轮的槽数
    size_t CurrentPos = 0;                      // 当前所在的槽
    std::chrono::seconds Si;                    // 一个槽代表一秒

    int epollfd;                                // 从属的epollfd


    // 还是需要使用管道，主要是因为sub reactor一直在监听，没有精力管时间轮的运行
    // 需要使用其他线程专门管时间轮，这个时候就需要文件描述符给epoll通知到时间了
    Chanel* tick_chanel;                        // 用于时间轮和外面通信的管道    
    int tick_d[2]{};                            // 用于tick时间轮的管道，0表示写1表示读

    std::vector<Timer*> todoList;               // 待删除队列

public:
    void tick();                                // 到时则执行计时器的函数

public:
    TimeWheel(int _epollfd, size_t maxsize = 12);
    ~TimeWheel();

    // 阻止赋值，即删除默认的拷贝构造函数与=运算符
    TimeWheel(const TimeWheel& Wheel) = delete;
    TimeWheel& operator=(const TimeWheel& wheel) = delete;

    // 时间轮操作函数
    Timer* TimeWheel_insert(std::chrono::seconds timeout,  HttpData* http);
    bool TimeWheel_remove(Timer* time);
    bool TimeWheel_adjust(Timer* timer, std::chrono::seconds timeout);

    Chanel* Get_tickChanel() {return tick_chanel;}
    int Get_1tick() {return tick_d[1];}

    // 时间轮的相应函数
    void Tw_callback();

};


// 时间轮子线程所需要使用的函数
void Run_Timer(std::vector<TimeWheel*> time_pool);

#endif