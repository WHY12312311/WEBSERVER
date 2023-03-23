#include "ThreadPool.h"
#include <iostream>


// 构造函数，直接在这构造出所有线程池
ThreadPool::ThreadPool(size_t size):isStop(false)
{
    // 使用for循环构造出size个线程，并定义线程的运行函数
    for (int i = 0; i != size; ++i){
        // emplace_back函数会调用vector中定义模板的构造函数，而std::thread
        // 的构造函数是可以使用lambda表达式的，相当于将lambda表达式传入了
        // std::thread的构造函数，并将构造出的结果直接放入了pool当中。
        pool.emplace_back([this](){
            // 下面就是每个线程的工作函数
            while (true){   // 如果不跳出则不停循环地取任务
                task tsk;
                {   // 我说这怎么有一个框呢，原来是为了自动解锁下面的lock

                    // 定义一个unique_lock来代管mutex锁
                    std::unique_lock<std::mutex> lock(this->mutex);
                    
                    // 这个wait函数是带谓词P的函数，使用lambda表达式来定义了
                    // P，而wait函数的执行步骤是当P不满足的时候，就不加锁并等待
                    // 条件变量收到信号。锁住或者谓词为否则阻塞。
                    // 反过来说就是满足P并且没锁的时候就加锁并继续执行。即下面的
                    // 语句表示要停止或者队列中有任务就加锁并运行。
                    this->cond.wait(lock, 
                    [this](){return this->isStop || this->task_queue.size();});

                    // 当要求停止并没有任务了，则退出函数，注意这个地方不用管锁，
                    // 因为unique_lock在退出作用域的时候会自动解锁
                    if (this->isStop && this->task_queue.empty())
                        return;

                    tsk = std::move(this->task_queue.front());
                    this->task_queue.pop_front();

                }   // 这个框一过就退出作用域了，顺便也解锁了unqiue_lock

                tsk();
            }
        });
    }
}

// 析构函数，本来我以为没有要析构的，结果是线程们需要进行析构
ThreadPool::~ThreadPool(){
    // 加锁并设置停止标志，这个地方加锁是因为锁也管得着任务队列，锁住不让加和取任务
    {   // 这次我知道是为了自动解锁了。
        std::unique_lock<std::mutex> lock(mutex); // 这个地方将lock加锁就会死锁，暂时不知道为啥
        isStop = true;
    }

    cond.notify_all();
    std::cout << "Thread joined......" << std::endl;

    // 回收子线程
    for (auto& thread : pool){  // 这里注意要使用引用，要不然因为没有合适的拷贝构造函数是不能用的。
        thread.join();
    }
}