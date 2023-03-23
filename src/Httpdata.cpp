#include <iostream>

#include "Httpdata.h"

// .............................SourceMap类.......................

// 要先将static变量实例化，否则会造成其无法识别
std::unordered_map<std::string,std::string> SourceMap::source_map{};
std::once_flag SourceMap::ocflag{};

void SourceMap::Init(){
    source_map[".html"] = "text/html";
    source_map[".avi"] = "video/x-msvideo";
    source_map[".bmp"] = "image/bmp";
    source_map[".c"] = "text/plain";
    source_map[".doc"] = "application/msword";
    source_map[".gif"] = "image/gif";
    source_map[".gz"] = "application/x-gzip";
    source_map[".htm"] = "text/html";
    source_map[".ico"] = "image/x-icon";
    source_map[".jpg"] = "image/jpg";
    source_map[".jpeg"] = "image/jpeg";
    source_map[".png"] = "image/png";
    source_map[".txt"] = "text/plain";
    source_map[".mp3"] = "audio/mp3";
    source_map["default"] = "text/html";
}

std::string SourceMap::Get_filetype(const std::string& file){
    // 初始化映射表，使用callonce保证每个线程只调用一次
    std::call_once(ocflag, Init);
    // 注意：这个地方会有多次访问，当客户端接收到html之后
    // 会根据html中的文件名来找资源，这时候实际上又是一次收发
    // 才能够拿到服务器上的图片资源
    if (source_map.count(file)) return source_map[file];
    else    return std::string();
}


// ............................HttpData类........................

HttpData::HttpData(Chanel* _curr_ch, EventLoop* _event_loop): curr_ch(_curr_ch), 
    event_loop(_event_loop){
    readbuf_size = GlobalValue::readbuf_size;
    writebuf_size = GlobalValue::writebuf_size;
    // memset(read_buf, 0, readbuf_size);
    // memset(write_buf, 0, writebuf_size);

    // 设置初始状态为解析请求行
    main_state = check_state_requestline;
    is_conn = true;

    // 直接注册四个操作
    Call_register(curr_ch);
}

HttpData::~HttpData(){
    
}

//......................... 注册操作..............................
void HttpData::Call_register(Chanel* chanel){
    chanel->HandlerRegister(Chanel::H_READ, [=]{Callback_read();});
    chanel->HandlerRegister(Chanel::H_WRITE, [=]{Callback_write();});
    chanel->HandlerRegister(Chanel::H_DISCONN, [=]{Callback_disconn();});
    chanel->HandlerRegister(Chanel::H_ERROR, [=]{Callback_error();});
    // std::cout << "Call back registered........" << std::endl;
}

void HttpData::Callback_read(){
    if (!curr_ch){
        std::cout << "No chanel for reading!" << std::endl;
        return;
    }
    bytes_read = Read_data(curr_ch->Get_fd(), read_buf, is_conn);

    
    // std::cout << read_buf << std::endl;

    // 解析http请求
    State_machine();
}

void HttpData::Callback_write(){
    // if (write_buf.empty())  return;
    // 写回数据
    int fd = curr_ch->Get_fd();
    int remain = write_buf.size()+1;
    bool isfull = 0;
    // std::cout << write_buf << std::endl;
    while (remain > 0){
        int curr_write = Write_data(fd, write_buf, isfull);
        if (curr_write < 0){    // 写入数据出错，直接断开连接
            Callback_disconn();
            return;
        }

        if (isfull && remain > 0){  // 写缓冲区满了，并且并没有写完
            // 这时候直接退出，等待下一次EPOLLOUT
            std::cout<<"send buffer full"<<std::endl;
            return;
        }
        remain -= curr_write;
    }

    // 取消EPOLLOUT，防止一直写数据
    curr_ch->Set_events_out(false);
    // 写完数据后要及时reset，通过长连接和短链接分情况处理
    Reset();
}

void HttpData::Callback_disconn(){
    std::cout << curr_ch->Get_fd() << ": Disconnecting..........." << std::endl;
    if (!curr_ch){
        std::cout << "No chanel for reading!" << std::endl;
        return;
    }
    // 时间轮在该函数之中也删除掉了
    event_loop->DelChanel(curr_ch);
}

void HttpData::Callback_error(){
    if (errno == 0) // 这个时候要么是报假警，要么就是已经处理过了，直接跳过
        return;
    perror("Connection error");
    Callback_disconn();
}

// ........................状态机解析...........................

/*  GET / HTTP/1.1
    Host: www.baidu.com
    User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:109.0) Gecko/20100101 Firefox/111.0
    Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,* /*;q=0.8
    Accept-Language: zh-CN,zh;q=0.8,zh-TW;q=0.7,zh-HK;q=0.5,en-US;q=0.3,en;q=0.2
    Accept-Encoding: gzip, deflate, br
    Connection: keep-alive
    Cookie: BAIDUID=DF4E9582F8955B366B0FAAA6832825AF:
    Upgrade-Insecure-Requests: 1
    Sec-Fetch-Dest: document
    Sec-Fetch-Mode: navigate
    Sec-Fetch-Site: none
    Sec-Fetch-User: ?1
*/

// 主状态机
void HttpData::State_machine(){
    // 先设置监听EPOLLOUT
    curr_ch->Set_events_out(true);
    // 标识是否完成或出错
    bool finish = false, error = false;
    while (!finish && !error){
        switch(main_state){
            case check_state_requestline: {
                sub_state = parse_requestline();
                switch(sub_state){
                    case requestline_data_is_not_complete:{ // 直接返回还是等待下一波数据？
                        return;
                    } break;
                    case requestline_parse_error: { // 设置错误信息，并continue准备下一次
                        Set_HttpErrorMessage(curr_ch->Get_fd(), 400, "Bad Request: Request line has syntax error");
                        error = true;
                        continue;
                    } break;
                    case requestline_is_ok: {   // 转换状态，进行下一步解析
                        main_state = check_state_header;
                        continue;
                    } break;
                }
            } break;

            case check_state_header: {
                sub_state = parse_header();
                switch (sub_state){     // 这里边的状态和前面检查请求行是一样的
                    case header_data_is_not_complete: {
                        return;
                    } break;
                    case header_parse_error: {
                        Set_HttpErrorMessage(curr_ch->Get_fd(), 400, "Bad Request: Request Header has syntax error");
                        error = true;
                        continue;
                    } break;
                    case header_is_ok: {
                        main_state = check_headerIsOk;
                        continue;
                    } break;
                }
            } break;

            case check_headerIsOk: {
                // 直接检查使用的method，以判断是post还是其他
                if (map["method"] == "POST" || map["method"] == "post") main_state = check_body;
                else    main_state = check_state_analyse_content;
                continue;
            } break;

            case check_body: {      // POST方法的请求带有请求体，是需要检查解析请求体的。
                // 设置时间轮，保证两个报文之间的事件不超过某个值
                event_loop->Get_TimeWheel()->TimeWheel_adjust(curr_ch->Get_timer(), GlobalValue::post_timeout);

                int content_length = 0;
                if (map.count("content-length") || map.count("Content-Length"))
                    content_length = std::stoi(map.count("content-length") ? map["ccontent-length"] : map["Count-Length"]);
                else {  // 没有这个数据代表出错了
                    Set_HttpErrorMessage(curr_ch->Get_fd(), 400, "Bad Request: Request body has syntax error");
                    error = true;
                    continue;
                }
                // 检查数据的完整性，如果不完整就重来
                if (read_buf.size() < content_length)
                    return; // 直接回退等待后续数据的到来
                else main_state = check_state_analyse_content;
            } break;

            case check_state_analyse_content: {
                if (!map.count("method")){
                    std::cout << "No method in this http data!" << std::endl;
                    return;
                }

                if(map["method"]=="post" || map["method"]=="POST")
                        sub_state=HttpData::Analyse_Post();
                    else if(map["method"]=="get" || map["method"]=="GET" || map["method"]=="head" || map["method"]=="HEAD")
                        sub_state=HttpData::Analyse_GetOrHead();
                    else {
                        return ;                                           //其他方法暂时不做处理
                    }

                    if(sub_state == analyse_error)
                    {
                        error=true;
                        break;
                    }

                    if(sub_state == analyse_success)
                    {
                        finish=true;
                        break;
                    }
            } break;

            // 发送相应报文
            Callback_write();
        }  
    }
}

sub_state_ParseHTTP HttpData::parse_requestline(){
    // 先找\r\n
    // str.find()函数返回第一个找到的位置，找不到则返回std::string::npos
    auto pos = read_buf.find("\r\n");
    if (pos == std::string::npos)   // 没找到
        return requestline_data_is_not_complete;
    
    // string.substr是从0开始取pos个，所以去到pos的前一位，pos没动
    std::string requestline = read_buf.substr(0, pos);
    read_buf = read_buf.substr(pos+2);  // 删除已取出的部分，+2是为了删掉\r\n

    // 使用正则表达式解析出请求行内容
        // 括号代表的是分组，^与$分别代表行头和行尾，其余的就是一些字符的匹配
    std::regex reg((R"(^(POST|HEAD|GET|post|head|get)\s(\S*)\s((HTTP|http)\/\d\.\d)$)"));
    std::smatch res;
    std::regex_match(requestline, res, reg);

    if (!res.empty()){
        // 注意res[0]代表的是整个匹配结果
        map["method"] = res[1];
        map["url"] = res[2];
        map["version"] = res[3];
        return requestline_is_ok;  
    }

    return requestline_parse_error;    
} 

sub_state_ParseHTTP HttpData::parse_header(){
    // 定义一个lambda表达式，用于解析每一行
    auto parse_a_line = [=](std::string str)->bool {
        std::regex reg(R"(^(\S*)\:\s(.*)$)");
        std::smatch res;
        std::regex_match(str, res, reg);
        if (res.size()) {
            map[res[1]] = res[2];
            return true;
        }
        return false;
    };

    // 循环地取出一行来解析
    int front = 0, rear = 0;
    while (true) {
        // 找到\r\n作为一行
        auto rear = read_buf.find("\r\n", front);
        if (rear == std::string::npos)   // 数据不完整，需要重新读取
            return header_data_is_not_complete;
        
        if (rear == front) {        // 解析到空行，也就是解析完成
            read_buf = read_buf.substr(rear+2);
            return header_is_ok;
        }        
        if (!parse_a_line(read_buf.substr(front, rear-front))){ // 数据出错
            return header_parse_error;
        }
        front = rear + 2;
    }
}

void HttpData::Set_HttpErrorMessage(int fd,int err_no,std::string msg){
    // 先将输出缓冲区给清除掉
    write_buf.clear();

    // 先输出一手错误信息
    std::cout << fd << " has a http error" << err_no << ": " << msg << std::endl;
    // printf("%d has a http error %d: %s", fd, err_no, msg);

    // 写输出报文到写缓冲区，使用html语言来写，这部分写的不精细，浏览器就会不显示结果
    // body
    std::string  response_body{};
    response_body += "<html><title>错误</title>";
    response_body += "<body bgcolor=\"ffffff\">";
    response_body += std::to_string(err_no)+msg;
    response_body += "<hr><em> Hust---WHY Server</em>\n</body></html>";

    //编写header
    std::string response_header{};
    response_header += "HTTP/1.1 " + std::to_string(err_no) + " " + msg + "\r\n";
    response_header += "Date: " + GetTime() + "\r\n";
    response_header += "Server: Hust---WHY\r\n";
    response_header += "Content-Type: text/html\r\n";
    response_header += "Connection: close\r\n";
    response_header += "Content-length: " + std::to_string(response_body.size())+"\r\n";
    response_header += "\r\n";

    write_buf =response_header + response_body;
    return ;
}

sub_state_ParseHTTP HttpData::Analyse_GetOrHead(){
    // 先写入回送报文中通用的部分
    Write_Response_GeneralData();

    // 解析文件名
    std::string filename = map["url"].substr(map["url"].find('/')+1);   // 找到第一个/后面的文件名
    std::string path = "../resource/" + filename;

    // 解析文件类型
    auto pos = filename.find('.');
    std::string file_type{};
    // std::cout << filename << filename.substr(pos) << std::endl;
    if (pos == std::string::npos)   file_type = "default";
    else                            file_type = SourceMap::Get_filetype(filename.substr(pos));

    if (file_type.empty()){ // 解析类型出错
        std::cout <<  filename.substr(pos) <<": No such a file type!" << std::endl;
        Set_HttpErrorMessage(curr_ch->Get_fd(), 404, "not found");
        return analyse_error;
    }
    write_buf += "content-type: " + file_type + "\r\n";

    // 解析文件大小
    struct stat file_info;
    if (stat(path.c_str(), &file_info) < 0){
        std::cout << curr_ch->Get_fd() << ": file not found" << std::endl;
        Set_HttpErrorMessage(curr_ch->Get_fd(), 404, "not found");
        return analyse_error;
    }

    int file_size = file_info.st_size;
    write_buf += "content-length: " + std::to_string(file_size) + "\r\n";

    // 首部结束，空一行作为分隔
    write_buf += "\r\n";

    // 对于head请求来说，不需要实体，直接返回即可
    if (map["method"] == "head" || map["method"] == "HEAD") return analyse_success;

    // 对于get请求来说，需要取出目标文件并返回
    // 打开文件，并创建内存映射
    int file_fd = open(path.c_str(), O_RDONLY);
    void* addr = mmap(0, file_size, PROT_READ, MAP_PRIVATE, file_fd, 0);
    close(file_fd); // 创建完内存映射就可以关上了

    if (addr == (void*)-1){ // 内存映射创建失败
        std::cout << file_fd << ": error mmapping!" << std::endl;
        munmap(addr, file_size);
        Set_HttpErrorMessage(curr_ch->Get_fd(), 404, "not found");
        return analyse_error;
    }

    // 创建的内存映射是被addr指向的，使用addr就可以取出
    char* buf = (char*)addr;
    write_buf += std::string(buf, file_size);   // 直接使用构造函数，创建一个临时字符串变量
    munmap(addr, file_size);
    return analyse_success;

}
sub_state_ParseHTTP HttpData::Analyse_Post(){
    Write_Response_GeneralData();

    //这里只简单地将读到的数据转换为大写

    std::string body=read_buf;
    for(int i=0;i<body.size();i++)
        body[i]=std::toupper(body[i]);

    write_buf += std::string("Content-Type: text/plain\r\n") + "Content-Length: " + std::to_string(body.size()) + "\r\n";
    write_buf += "\r\n" + body;

    return analyse_success;
}

// 编写报文中通用的部分
void HttpData::Write_Response_GeneralData(){
    std::string status_line = map["version"] + " 200 OK\r\n";
    std::string header_line{};
    header_line += "Date: "+ GetTime() + "\r\n";
    header_line += "Server: Hust---WHY\r\n";

    if(map["Connection"]=="Keep-Alive" || map["Connection"]=="keep-alive")
    {
        header_line += "Connection: keep-alive\r\n"
            + std::string("Keep-Alive: ") + std::string(std::to_string(GlobalValue::conn_timeout.count())) +"s \r\n";
    }
    else
    {
        header_line += "Connection: Connection is closed\r\n";
    }

    write_buf = status_line + header_line;
    return ;
}

void HttpData::Reset_Http_events(bool in){
    // 问题：这个我好像没必要写
}
void HttpData::Reset(){
    // 判断长连接还是短链接，短链接则直接关闭
    if (map["Connection"]=="keep-alive" || map["Connection"]=="Keep-Alive"){
        // 设置时间轮以保证规定时间内无消息则关闭
        event_loop->Get_TimeWheel()->TimeWheel_adjust(curr_ch->Get_timer(), GlobalValue::conn_timeout);
    }
    else {
        Callback_disconn();
        return;
    }

    // 长连接还得清除各种缓冲区
    read_buf.clear();
    write_buf.clear();
    map.clear();
    main_state = check_state_requestline;
    return;
}