#include <iostream>

#include "Timer.h"


// Timer类

Timer::Timer(size_t pos, size_t turns, HttpData* _http):PosInWheel(pos), Turns(turns), time_http(_http) {
        Timer_SetCall([=] {time_http->Callback_disconn();});
}

void Timer::DoCallback(){
    if (FuncOfTimeUp)   FuncOfTimeUp();
    else std::cout << "No Callback function!" << std::endl;
}

// TimeWheel类

TimeWheel::TimeWheel(int _epollfd, size_t maxsize):epollfd(_epollfd), SizeOfWheel(maxsize), 
    Si(GlobalValue::SlotTime){
    // 初始化空的槽
    slot = std::vector<std::list<Timer*>>(SizeOfWheel, std::list<Timer*>());

    // 创建一个管道用于时间轮的tick
    int pipefd = pipe(tick_d);
    if (pipefd == -1){
        std::cout << "TimeWheel create pipe error" << std::endl;
        exit(-1);
    }
    // std::cout << tick_d[0] << " " << tick_d[1] << std::endl;
    // 设置管道的两端非阻塞
    SetNonBlocking(tick_d[0]);
    SetNonBlocking(tick_d[1]);

    // 创建Chanel，注意这个管道是监听的0。
    tick_chanel = new Chanel(tick_d[0], epollfd, false, nullptr);
    tick_chanel->Set_events(EPOLLIN);
    // 原来在这等着我呢，这个地方定义了每一次监听到管道读事件就tick一下
        // 后面是一个lambda表达式，[=]代表的是自动推导捕获列表
    tick_chanel->HandlerRegister(Chanel::H_READ, [=]{Tw_callback();});

}

TimeWheel::~TimeWheel(){
    // 先清空所有的槽
    // TODO：这部分原版代码使用了delete，但是我觉得没必要，先这样试试

    // 关闭管道与文件描述符
    delete tick_chanel;
    //close(tick_d[0]);   // 实际上这一头应该有Chanel关闭过了
    close(tick_d[1]);
}

Timer* TimeWheel::TimeWheel_insert(std::chrono::seconds timeout, HttpData* http){
    // 如果时间小于0则直接返回空
    if (timeout <= std::chrono::seconds(0))
        return nullptr;
    // 以时间轮当前位置为基准，应该向前多少轮和多少位置
    size_t cycle = 0, pos = 0;
    if (timeout < Si)   cycle = 0, pos = 1;
    else{
        cycle = (timeout/Si)/SizeOfWheel;
        pos = (CurrentPos+(timeout/Si)%SizeOfWheel)%SizeOfWheel;
    }

    // 创建新的Timer，并根据pos将其插入slot中
    Timer* newTimer = new Timer(pos, cycle, http);
    slot[pos].emplace_back(newTimer);

    return newTimer;
}

bool TimeWheel::TimeWheel_remove(Timer* timer){
    if(!timer)  return false;
    // remove函数根据值来删除，自动查找相同的值并都删掉
    slot[timer->PosInWheel].remove(timer);
    delete timer;   // timer都是使用new定义的，所以必须给delete掉
    return true;
}

bool TimeWheel::TimeWheel_adjust(Timer* timer, std::chrono::seconds timeout){
    // 如果时间小于0则直接返回空
    if (timeout <= std::chrono::seconds(0))
        return false;
    // 以时间轮当前位置为基准，应该向前多少轮和多少位置
    size_t cycle = 0, pos = 0;
    if (timeout < Si)   cycle = 0, pos = 1;
    else{
        cycle = (timeout/Si)/SizeOfWheel;
        pos = (CurrentPos+(timeout/Si)%SizeOfWheel)%SizeOfWheel;
    }

    // 从现在的槽中删除并插入到新的槽
    slot[timer->PosInWheel].remove(timer);
    timer->Timer_SetPos(pos);
    timer->Timer_SetTurns(cycle);
    slot[pos].emplace_back(timer);

    return true;
}

void TimeWheel::tick(){
    // 将所有的到时任务都放到待办中
    for (auto itr = slot[CurrentPos].begin(); itr != slot[CurrentPos].end(); ++itr){
        if (!*itr)
            continue;
        Timer* p = *itr;
        // 还有圈数就代表还不该执行
        if (p->Timer_GetTurns() > 0){
            p->Timer_TurnsDecline();
            continue;
        } else {
            todoList.emplace_back(p);
        }
    }
    // 切换到下一个槽
    CurrentPos = (CurrentPos+1)%SizeOfWheel;
    // 如果队列中有数据，则向管道中写入，以激活监听的epoll
    if (todoList.size()){
        int res = write(tick_d[1], "pipe\0", sizeof "pipe\0");
        if (res < 0){
            perror("write");
        }
    }
}

void TimeWheel::Tw_callback(){
    // 需要先读入管道中的数据，防止积攒过多堵住
    std::string buf{};
    read(tick_d[0], (void*)buf.c_str(), 100);   // 由于每次写入的很少，这里就定量100字节读了
    std::cout << buf << std::endl;

    // 每一个到时的计时器都调用超市回调函数
    for (auto itr : todoList){
        itr->DoCallback();
        // TimeWheel_remove(itr);
    }
    todoList.clear();
}

void Run_Timer(std::vector<TimeWheel*> time_pool){
    // 需要循环地监听时间轮中的计时器是否到期
        // 使用chrono可以减小系统的负担
    auto start_time = std::chrono::steady_clock::now();
    while (true){
        auto cur_time = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(cur_time - start_time).count() >= 1){
            for (int i = 0; i != time_pool.size(); ++i){
                TimeWheel* tw = time_pool[i];
                tw->tick();
            }
            start_time = cur_time;
        }
    }
}