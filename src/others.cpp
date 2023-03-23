#include "others.h"


// 全局变量定义区
int GlobalValue::SlotTime = 1;
int GlobalValue::ListenQueue = 10;
int GlobalValue::readbuf_size = 4096;
int GlobalValue::writebuf_size = 4096;
std::chrono::seconds GlobalValue::conn_timeout = std::chrono::seconds(15);
std::chrono::seconds GlobalValue::post_timeout = std::chrono::seconds(30);

// 设置文件描述符非阻塞
void SetNonBlocking(int fd){
    int flags = fcntl(fd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}

// 解析命令行函数
std::optional<std::tuple<int, int, std::string>> GetCommand(int argc, char ** argv){
    if (argc <= 4)
        return std::nullopt;
    // 定义模式
    std::string optstring = "p:s:l:";
    char res = '\0';
    int port, Reactorsize;
    std::string logPath;
    while ((res = getopt(argc, argv, optstring.c_str())) != -1){
        switch (res){
            case 'p':
                port = atoi(optarg);
                break;
            case 's':
                Reactorsize = atoi(optarg);
                break;
            case 'l': {
                // 这个地方要将路径写进去
                // R()代表后面的字符串不进行转义，即使用原汁原味的字符串
                // ^表示开始字符串匹配，\.与\/代表.与/，因为正则表达式中
                // ./经常被用作匹配，所以要匹配这两个字符就还得转义
                // \S代表匹配任意不是空格的字符串，*表示匹配任意次。
                std::regex Reg_path(R"(^\.\/\S*)");
                std::smatch result{};
                std::string path(optarg);
                std::regex_match(path, result, Reg_path);
                if (result.empty()){
                    std::cout << "Log path illegal!" << std::endl;
                    return std::nullopt;
                }
                logPath = result[0];
            }   // 使用大括号来规定作用域，switch语句中声明变量是需要这样的，否则无法自动销毁
                break;
            default:
                break;
        }
    }
    return std::tuple<int, int, std::string>(port, Reactorsize, logPath);
}


// 写入数据
int Write_data(int fd, std::string& content, bool& isfull){
    int bytes_write = 0;
    const char* p = content.c_str();    // c_str函数自动在末尾加一个'\0'
    int remain = content.size() + 1;

    // 由于是ET模式非阻塞地写，需要使用while循环来写
    while (remain){
        int curr_byte = send(fd, p, remain, 0);
        if (curr_byte < 0){
            if (errno == EAGAIN || errno == EWOULDBLOCK){   // 写缓冲区满了
                errno = 0;  // 清空errno，防止其他地方报错
                isfull == true;
                return bytes_write;  // 满了的话就break去最后处理，将剩下的留下来等待下次处理
            }
            else if (errno == EINTR){   // 被系统调用中断
                errno = 0;  // 清空errno，防止其他地方报错
                continue;
            }
            else {
                perror("send");
                return -1;
            }
        }
        else if (curr_byte == 0){   // 断开连接了
            return bytes_write;
        }
        bytes_write += curr_byte;
        remain -= curr_byte;
        p += curr_byte;
    }

    // 判断是否写完，没写完的话就将剩下的留下，并准备下次写
    if (content.size()+1 == bytes_write) content.clear();
    else content = content.substr(bytes_write);
    // 问题：这个地方需要在这注册一遍EPOLLOUT吗？
    return bytes_write;
}


// 读取数据
int Read_data(int fd, std::string& read_buf, bool& is_conn){
    int bytes_read = 0;
    char buf[GlobalValue::readbuf_size];
    memset(buf, 0, sizeof buf);
    while (true){   // 一次全给读了
        /*!
         对非阻塞I/O：
         1.若当前没有数据可读，函数会立即返回-1，同时errno被设置为EAGAIN或EWOULDBLOCK。
           若被系统中断打断，返回值同样为-1,但errno被设置为EINTR。对于被系统中断的情况，
           采取的策略为重新再读一次，因为我们无法判断缓冲区中是否有数据可读。然而，对于
           EAGAIN或EWOULDBLOCK的情况，就直接返回，因为操作系统明确告知了我们当前无数据
           可读。
         2.若当前有数据可读，那么recv函数并不会立即返回，而是开始从内核中将数据拷贝到用
           户区，这是一个同步操作，返回值为这一次函数调用成功拷贝的字节数。所以说，非阻
           塞I/O本质上还是同步的，并不是异步的。
         */
        // errno == EAGAIN || errno == EWOULDBLOCK的时候会报错Resource temporarily unavailable
        // 然后epoll就会收到一个EPOLLERR，解析出来就是这个报错，应该在这处理的时候及时将errno置为0
        int curr_read = recv(fd, buf, sizeof buf, 0);
        if (curr_read < 0){
            if (errno == EAGAIN || errno == EWOULDBLOCK){    // 正常读完了
                errno = 0;  // 清空errno，防止其他地方报错
                break;
            }
            else if (errno == EINTR) {   // 系统中断，直接继续读就好了
                errno = 0;  // 清空errno，防止其他地方报错
                continue;
            }
            else {
                perror("Read_data");
                return -1;
            }
        }
        else if (curr_read == 0){
            is_conn = false;
            std::cout << fd << " has disconnected........" << std::endl;
            break;
        }
        else {
            bytes_read += curr_read;
            read_buf += buf;    // 直接这样就可以写入了
        }
    }
    return bytes_read;
}


int bindNlisten(__uint32_t portnum){
    int listenfd;
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        return -1;
    }

    // 设置端口复用，注意：端口复用必须在bind之前设置！！！！
    int reuse = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0){
        perror("setsockopt");
        exit(-1);
    }


    // 设置socket地址
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(portnum);
    if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        perror("bind");
        return -1;
    }

    if (listen(listenfd, GlobalValue::ListenQueue) < 0){
        perror("listen");
        return -1;
    }

    return listenfd;
}


std::string GetTime(){

    time_t lt;  // 是一个整数类型，用于存储从1970年到现在经过了多少秒
    lt = time(&lt);

    struct tm* ptr; // 该结构体里面是一串int，保存了各种事件和日期
    ptr = localtime(&lt);

    // strftime可以将tm类型的结构体时间按照规定的格式打印出来。
    char time_buf[100];
    strftime(time_buf, 100, "%a, %d %b %Y %H:%M:%S", ptr);

    return std::string(time_buf);
}


// 辅助注册信号处理函数
void Sig_register(int signum, void(handler)(int)) {   // 需要注意函数指针的写法
    struct sigaction act;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    act.sa_handler = handler;
    if (sigaction(signum, &act, NULL) == -1){
        perror("sigaction");
        exit(-1);
    }
}