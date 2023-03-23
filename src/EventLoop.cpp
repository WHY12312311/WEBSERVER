// 这个地方实际上都是为了subreator写的，mainreactor是在server类中处理的

#include "EventLoop.h"


EventLoop::EventLoop(bool ismain):isMainReactor(ismain) {
    // 注册epoll监听事件
    if ((epollfd = epoll_create(1)) < 0){
        perror("epoll_create");
        exit(-1);
    }
    
    // http_data = new HttpData(this);
    Conn_num = 0;
    stop = false;

    time_wheel = new TimeWheel(epollfd);

    // 将时间轮中的管道加入监听，也就相当于将时间轮加入了监听
    AddChanel(time_wheel->Get_tickChanel());
}

EventLoop::~EventLoop(){
    close(epollfd);
    delete time_wheel;
    // delete http_data;
    // 其他成员变量都有自己的析构函数
}

// 向当前的loop中添加新的监听
bool EventLoop::AddChanel(Chanel* CHNL){
    if (!CHNL){
        std::cout << "Fail to add chanel!" << std::endl;
        return false;
    }

    int newfd = CHNL->Get_fd();
    chanelpool[newfd] = CHNL;

    // 设置文件描述符非阻塞
    SetNonBlocking(newfd);

    // 加入epoll
    // EPOLLONTSHOT模式相当于给fd加锁，防止其他线程同时读取这个描述符
        // 3/15：bug找了半小时，结果发现每次要设置events都想着在别的地方设定，最终也没
        // 想好在哪设定，然后就搁置了，导致跑的时候根本无法监听事件。
    // 刚开始注册的时候还不需要监听EPOLLOUT，否则会一直写空消息
    CHNL->Set_events(EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLET | EPOLLONESHOT);

    // 如果是时间轮的Chanel，到这就可以了
    if (CHNL == time_wheel->Get_tickChanel())
        return true;

    // 加入时间轮，方便监听
    Timer* tmp = time_wheel->TimeWheel_insert(GlobalValue::conn_timeout, CHNL->Get_http());
    CHNL->Set_timer(tmp);

    // 设置好reactor
    CHNL->Set_loop(this);
    

    // 管道与main reactor的不需要注册，在各自定义的地方已经注册了
    // 这俩也不参与连接数计数
    if (CHNL != time_wheel->Get_tickChanel() && !isMainReactor){
        // http_data->Call_register(CHNL);
        // std::cout << "connection add......" << std::endl;
        // 计数，这个地方不需要临界区，因为一个线程只对应一个reactor
        ++Conn_num;
    }

    return true;
}

bool EventLoop::ModChanel(Chanel* CHNL, __uint32_t EV){
    if (!CHNL){
        std::cout << "Fail to modify chanel!" << std::endl;
        return false;
    }

    // int fd = CHNL->Get_fd();
    // epoll_event newev;
    // newev.data.fd = fd;
    // newev.events = EV;
    // epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &newev);

    // 直接交给下面的函数执行了
    CHNL->Set_events(EV);
    return true;
}

bool EventLoop::DelChanel(Chanel* CHNL){
    if (!CHNL){
        std::cout << "Fail to delete chanel!" << std::endl;
        return false;
    }

    // 删除时间轮中的注册
    time_wheel->TimeWheel_remove(CHNL->Get_timer());

    // 删除epoll中的注册
    int ret = epoll_ctl(epollfd, EPOLL_CTL_DEL, CHNL->Get_fd(), NULL);
    if (ret < 0){
        perror("epoll delete");
        return false;
    }
    // 清除其占用的资源
    chanelpool[CHNL->Get_fd()] = nullptr;
    CHNL->~Chanel(); // 这个地方应该直接析构就可以了，也不打算复用信道
    if (typeid(CHNL) == typeid(void))   // 如果是由new动态分配的则使用delete删除掉
        delete CHNL;
    
    // 计数，删除要减一
    --Conn_num;
    return true;
}
    
int EventLoop::Get_Conn_Num(){ 
    return Conn_num;
}

void EventLoop::StartLoop(){
    while (!stop){
        ListenNCall();
    }
}

void EventLoop::StopLoop(){
    stop = false;
    // 销毁所有chanel
    for (auto itr : chanelpool){
        DelChanel(itr);
    }
}

void EventLoop::ListenNCall(){
    // 如果还没有要监听的fd就不着急开epoll
    while (isMainReactor || (!stop && Conn_num)){
        // 阻塞地监听，并设置超时时间
        int res_num = epoll_wait(epollfd, events, MaxEvent, Epoll_timeout);
        if (res_num == 0){
            std::cout << "Epoll timout!" << std::endl;
            continue;
        }else if (res_num < 0){
            perror("epoll_wait");
            stop = true;
        }
        for (int i = 0; i != res_num; ++i){
            int currfd = events[i].data.fd;
            // 差点忘记了这里在Chanel类封装了回调函数之类的了
            Chanel* currchanel = chanelpool[currfd];
            if (currchanel){
                currchanel->Set_revents(events[i].events);
                currchanel->Callrevents();
                // 在调用完操作函数之后就需要重置EPOLLONTSHOT事件
                currchanel->Set_oneshot();
            }
            else {
                std::cout << i << ": no related chanel for this fd!" << std::endl;
            }
        }
    }
}