#include "Server.h"

// 必须在cpp文件中定义一下，否则链接器找不到
// 原因是static关键字修饰的成员变量必须先进行实例化才能使用
SERVER* SERVER::server = nullptr;
std::vector<TimeWheel*> SERVER::time_pool{};

SERVER::SERVER(int _port, int num_sub, ThreadPool* _tp, EventLoop* _ep){
    listenfd = bindNlisten(_port);
    thread_pool = _tp;
    main_loop = _ep;

    // 创建多个sub，初始化eventpool
    for (int i = 0; i != num_sub; ++i){
        std::shared_ptr<EventLoop> ptr = std::make_shared<EventLoop>(false);
        eventpool.emplace_back(ptr);
        time_pool.emplace_back(ptr->Get_TimeWheel());   // 往两个池子中加入时间轮

        // 初始化小顶堆，堆中元素的格式是编号-连接数量
        Lheap.push(Node(i, 0));
    }

    listen_ch = new Chanel(listenfd, main_loop->Get_epollfd(), false, main_loop);

    // 差点忘记了给listenfd设置非阻塞
    SetNonBlocking(listenfd);
}

SERVER::~SERVER(){
    delete listen_ch;
    close(listenfd);
    // delete server应该是不需要的，因为这个地方相当于是main函数调用的，main函数来清除
}

// 
SERVER* SERVER::server_init(int _port, int num_sub, ThreadPool* _tp, EventLoop* _ep){
    if (!server){
        {   // if语句和下面的lock都是保证单例只会被创建一次
            std::unique_lock<std::mutex>(lock);
            if (!server) // 再检查一遍，以防上次检查之后加锁之前有其他线程创建
                server = new SERVER(_port, num_sub, _tp, _ep);
        }
    }
    return server;
}


// SERVER具体承担什么任务？
// 使用epoll监听客户端的连接请求
// 将客户端的请求分发给sub
// 运行所有sub
void SERVER::Start_server(){
    // 运行所有sub !!!bug: 这个地方应该给子线程跑，怎么能让main reactor跑呢？
    // for (auto itr : eventpool){
    //     itr->StartLoop();
    // }

    // 设置并运行mainloop
    listen_ch->HandlerRegister(Chanel::H_READ, [=]{Conn_handler();});
    listen_ch->HandlerRegister(Chanel::H_ERROR, [=]{Err_handler();});
    main_loop->AddChanel(listen_ch);

    // 将subreactor加入任务队列，让线程池来运行
    for (int i = 0; i != eventpool.size(); ++i){
        thread_pool->Addtask([=]{eventpool[i]->StartLoop();});
    }

    main_loop->StartLoop();

}

void SERVER::Stop_server(){
    for (auto itr : eventpool){
        itr->StopLoop();
    }
    main_loop->StopLoop();
    // 关闭listenfd
    main_loop->DelChanel(listen_ch);
}

// main reactor在接收到连接信号的时候，需要将新的连接分发给下面
void SERVER::Conn_handler(){
    // 接受连接，由于每次只会通知一个连接，所以没有循环的必要
    int connfd;
    struct sockaddr_in retaddr;
    socklen_t retlen = sizeof(retaddr);
    if ((connfd = accept(listenfd, (struct sockaddr*)&retaddr, &retlen)) < 0){
        Getlogger()->error("Socket accept error: {}", strerror(errno));
        exit(-1);
    }

    // 分发机制，需要使用锁来保护堆
    // int idx = -1;
    // {
    //     while (Lheap.top() == Dheap.top()){ // 使用删除队列进行删除
    //         Lheap.pop();
    //         Dheap.pop();
    //     }

    //     Node tmp = Lheap.top();
    //     Lheap.pop();
    //     idx = tmp.key;
    //     ++tmp.value;
    //     Lheap.push(tmp);
    // }

    // 分发机制，直接遍历所有的sub，找到其中最小的一个
    int idx = -1, min_conn = 10000, sum_conn = 0;
    for (int i = 0; i != eventpool.size(); ++i) {
        int tmp = eventpool[i]->Get_Conn_Num();
        sum_conn += tmp;
        if (tmp <= min_conn){
            min_conn = tmp;
            idx = i;
        }
    }

    // 限制最大并发连接数
    if (sum_conn >= GlobalValue::Maxconn){
        bool isfull = false;
        std::string str("Connection is currently unavalable");
        Write_data(connfd, str, isfull);
        Getlogger()->warn("{}: Max connection number!", connfd);
        return;
    }
    

    // 输出连接信息
    Getlogger()->info("{}: The port of new connection is: {}", connfd, ntohs(retaddr.sin_port));
    auto newchanel = new Chanel(connfd, eventpool[idx]->Get_epollfd(), true, eventpool[idx].get());
    
    // 构造函数中没有设置监听，所以要在这设置好监听再加入
    // ADD函数中有下面的语句，不需要在这指定了
    // newchanel->Set_events(EPOLLIN | EPOLLRDHUP | EPOLLHUP);

    
    // 插入
    eventpool[idx]->AddChanel(newchanel);
}

void SERVER::Err_handler(){
    Getlogger()->error("Setver fatal error: {}", strerror(errno));
    exit(-1);
}