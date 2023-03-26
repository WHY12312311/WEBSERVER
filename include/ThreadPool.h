// 剖析花哨线程池：https://www.cnblogs.com/chenleideblog/p/12915534.html
// PS：这辈子都不想再看一遍这一坨代码了
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <future>
#include <memory>

class ThreadPool{
public:
    ThreadPool(size_t size);                // 构造函数，其中size是指该线程池中有多少个线程
    ~ThreadPool();
    template<class F, class... Args>        // 向工作队列中加入任务
        auto Addtask(F &&f, Args &&...args)
        ->std::future<typename std::result_of<F(Args...)>::type>;
    void Stop_thread()   {isStop = true;}
    
private:
    typedef std::function<void()> task;     // 将task定义为一类没有输出没有输入的函数

private:
    std::mutex mutex;                       // 互斥锁
    std::condition_variable cond;           // 条件变量
    std::deque<task> task_queue;            // 任务队列，其中的任务是上面定义类型的函数
    std::vector<std::thread> pool;          // 线程池，使用vector来存放
    bool isStop;                            // 线程池是否需要停止
};


// 这个玩意估计是函数名太长了，放在cpp里链接器直接就是一手找不到
// 向工作队列中添加工作
template<class F, class... Args>
auto ThreadPool::Addtask(F &&f, Args &&...args)
    ->std::future<typename std::result_of<F(Args...)>::type>    // 相当于返回值是一个放在future里的异步执行结果
{
    using return_type = typename std::result_of<F(Args...)>::type;

    // tsk是一个函数指针，指向使用bind函数封装的函数
    // 使用bind函数可以将多种不同类型的函数转变为void name()类型
    auto tsk = std::make_shared<std::packaged_task<return_type()>>
        (std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    // res保存的是future类型的异步执行结果。但是异步执行结束之后才会将结果放入res中
    // 因为future是异步执行的，所以tsk执行不执行不影响当前函数的执行，会继续执行
    std::future<return_type> res = tsk->get_future();

    {
        std::unique_lock<std::mutex> lock(mutex);

        if (isStop)
            throw std::runtime_error("The threadpool is already stopped!");
        // 新任务入队
        task_queue.emplace_back([tsk](){
            // 这地方应该是先将函数指针解出来再运行，相当于使用lambda表达式定义的函数中调用了tsk函数。
            (*tsk)();   // 是写的真花哨啊
        });
    }
    
    cond.notify_one();

    return res; // 这个地方返回的是tsk异步执行结果，即future里面的值
}

#endif