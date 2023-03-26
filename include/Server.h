// 这个SERVER类就像是一个特殊的EventLoop类，即mainreactor
#ifndef SERVER_H
#define SERVER_H

#include <memory>
#include <mutex>
#include <typeinfo>
#include <queue>
#include <new>

#include "ThreadPool.h"
#include "EventLoop.h"
#include "Chanel.h"

class ThreadPool;
class EventLoop;
class Chanel;

class SERVER{

private:
    // sock连接
    int listenfd;
    int port;

    int idx = 0;

    // 线程池和Mainreactor
    ThreadPool* thread_pool;
    EventLoop* main_loop;

    // subreactor智能指针数组
    std::vector<std::shared_ptr<EventLoop>> eventpool;                      // 用智能指针，可以自动析构

    // 堆的数据元素类型
    struct Node{
        int key;
        int value;
        Node(int k, int v):key(k), value(v) {}
        bool operator<(const Node& n) const {       // 声明为const成员才可以，因为堆默认是const的
            if (n.value != value)
                return n.value < value;
            else 
                return n.key < key;
        }
        bool operator==(const Node& n) const {
            return (n.value == value && n.key == n.key);
        }
    };

    // 使用小顶堆来实现轮询算法
    std::priority_queue<Node> Lheap, Dheap;

    //使用懒汉模式来单例
    static SERVER* server;
    std::mutex lock;    // 用这个锁来保护这个单例的SERVER
    Chanel* listen_ch;  // main reactor使用的chanel

    // 单例模式私有的构造函数
    SERVER(int _port, int num_sub, ThreadPool* _tp, EventLoop* _ep);

    // 是否需要定义私有类来进行析构？私有类如果放在栈区，可以在程序结束的时候调用析构函数来析构。

public:
    ~SERVER();

    // 对外的创建接口，必须设置成static，要不然报错“非静态成员引用必须与特定的对象相对应”
    static SERVER* server_init(int _port, int num_sub, ThreadPool* _tp, EventLoop* _ep);

    // 开始和结束服务器
    void Start_server();
    void Stop_server();

    // 直接就是一手时间轮池子
    static std::vector<TimeWheel*> time_pool;

private:
    // 处理一下错误和连接请求
    void Conn_handler();
    void Err_handler();
};

#endif