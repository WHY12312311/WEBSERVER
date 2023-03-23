#include <iostream>

#include "Chanel.h"

Chanel::Chanel(int _fd, int _epollfd, bool _is_conn, EventLoop* _eventloop):fd(_fd), 
    epollfd(_epollfd), is_connect(_is_conn), eventloop(_eventloop){
    // 已经迁移到了HttpData类中
    // memset(read_buf, 0, readbuf_size);
    // memset(write_buf, 0, writebuf_size);
    is_events = false;
    httpdata = new HttpData(this, eventloop);
}

Chanel::~Chanel(){
    delete httpdata;    // 使用new创建的需要使用delete删除掉
    close(fd);
}

// 注意以下几种epoll状态，特别需要注意EPOLLRDHUP在发送的时候会顺便发送一个EPOLLIN
    // EPOLLIN：表示对应的文件描述符可以读（包括对端socket正常关闭）；
    // EPOLLOUT：表示对应的文件描述符可以写；
    // EPOLLRDHUP：表示对端socket关闭连接，或者半关闭状态（即只关闭了其中的一端）；
    // EPOLLPRI：表示对应的文件描述符有紧急数据可读；
    // EPOLLERR：表示对应的文件描述符发生错误；
    // EPOLLHUP：表示对应的文件描述符被挂断；
    // EPOLLET：设置事件的触发方式为边缘触发（Edge Triggered）；
    // EPOLLONESHOT：设置事件的触发方式为仅触发一次。
void Chanel::Callrevents(){
    // 在ET模式下，每次监测到事件都会汇报，所以这里使用if就可以，各个事件是不重复的
        // 需要特别注意处理这几个事件的顺序，有的版本中在发送EPOLLRDHUP的时候会同时发送
        // 一个EPOLLIN，如果先处理EPOLLIN就有可能会出现内存泄漏，因为这时候已经半关闭了
    if (revents & EPOLLERR){
        // std::cout << "EPOLLERR" << std::endl;
        CallHandler(H_ERROR);
    }
    
    else if (revents & EPOLLHUP || revents & EPOLLRDHUP){    // bug：每一次收发请求之后都会触发EPOLLRDHUP，致使没有长连接存在
        std::cout << "Client closed........" << std::endl;
        CallHandler(H_DISCONN);
    }
        
    else if (revents & EPOLLIN){
        // std::cout << "EPOLLIN" << std::endl;
        CallHandler(H_READ);
    }

    else if (revents & EPOLLOUT){
        // std::cout << "EPOLLOUT" << std::endl;
        CallHandler(H_WRITE);
    }
}

void Chanel::CallHandler(handler_enum Hnum){
    switch(Hnum){
        // TODO：加上日志
        case H_READ:
            if (read_handle != nullptr){
                read_handle();
            }else {
                std::cout << "No read handler for this chanel!" << std::endl;
                exit(-1);
            }
            break;
        case H_WRITE:
            if (write_handle != nullptr){
                write_handle();
            }else {
                std::cout << "No write handler for this chanel!" << std::endl;
                exit(-1);
            }
            break;
        case H_ERROR:
            if (error_handle != nullptr){
                error_handle();
            }else {
                std::cout << "No error handler for this chanel!" << std::endl;
                exit(-1);
            }
            break;
        case H_DISCONN:
            if (disconn_handle != nullptr){
                disconn_handle();
            }else {
                std::cout << "No disconnect handler for this chanel!" << std::endl;
                exit(-1);
            }
            break;
        default:
            std::cout << "No such a enum!" << std::endl;
            exit(-1);
            break;
    }
}

void Chanel::HandlerRegister(handler_enum Hnum, CALLBACK fun){
    switch(Hnum){
        case H_READ:
            read_handle = std::move(fun);
            break;
        case H_WRITE:
            write_handle = std::move(fun);
            break;
        case H_ERROR:
            error_handle = std::move(fun);
            break;
        case H_DISCONN:
            disconn_handle = std::move(fun);
            break;
        default:
            std::cout << "No such a enum!" << std::endl;
            exit(-1);
    }
}


void Chanel::Set_events_out(bool isout){
    __uint32_t newev = EPOLLIN | EPOLLERR | EPOLLRDHUP | EPOLLHUP | EPOLLET | EPOLLONESHOT;
    if (isout)  newev |= EPOLLOUT;
    Set_events(newev);
}

