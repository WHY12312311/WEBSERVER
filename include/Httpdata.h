#ifndef HTTPDATA_H
#define HTTPDATA_H

#include <unordered_map>
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#include "Chanel.h"
#include "EventLoop.h"
#include "others.h"

// 类前置声明
class EventLoop;
class Chanel;

class SourceMap {

private:
    // 初始化，只调用一次
    static void Init();
    SourceMap() = default;

    // 定义成全局的，保证只生成一个映射
    static std::once_flag ocflag;
    static std::unordered_map<std::string, std::string> source_map;

public:
    // 禁止拷贝构造
    SourceMap(SourceMap& old) = delete;
    SourceMap& operator=(SourceMap& old) = delete;
    // 对外调用接口
    static std::string Get_filetype(const std::string& file);

};

//主状态机
enum main_State_ParseHTTP{check_state_requestline, check_state_header, check_headerIsOk, check_body, check_state_analyse_content};

//从状态机
enum sub_state_ParseHTTP{
    requestline_data_is_not_complete, requestline_parse_error, requestline_is_ok,       //这一行表示的是解析请求行的状态
    header_data_is_not_complete, header_parse_error, header_is_ok,                      //这一行表示的是解析首部行的状态
    analyse_error,analyse_success                                                       //分析报文并填充发送报文状态
};


// 处理http事件
    // 这部分反复横跳了好多遍，最终决定还是需要这个类。不将这个类作为一个纯操作类
    // 而是决定将这个类作为一个辅助Chanel的类，和Chanel深度耦合
    // 起初是觉得Chanel类就可以完成所有的功能，没有必要再开一个类，并且害怕这个类
    // 对象的创建和析构会造成不必要的开销，细分析之后发觉这个类是和Chanel
    // 一起创建和销毁的，并不会产生额外的开销。
    // 实际上这个类和Chanel类相当于同一个类拆开，这里用于解析，而Chanel用于处理调用
    // 相当于一个外层一个内层
class HttpData{

private:
    bool is_conn;                                       // 标志是否处于连接状态

    Chanel* curr_ch;                                    // 当前处理的Chanel
    EventLoop* event_loop;                              // 从属的reactor

    int bytes_read;                                     // 读取数量
    int bytes_write;                                    // 写入数量

    int readbuf_size = GlobalValue::readbuf_size;       // 读缓冲区大小
    int writebuf_size = GlobalValue::writebuf_size;     // 写缓冲区大小

    std::string read_buf;                               // 读缓冲区
    std::string write_buf;                              // 写缓冲区

    main_State_ParseHTTP main_state;                    // 主状态
    sub_state_ParseHTTP sub_state;                      // 从状态

    std::unordered_map<std::string, std::string> map;   // 保存解析结果

public:
    HttpData(Chanel* _curr_ch, EventLoop* _event_loop);
    ~HttpData();

    // void Set_Chanel(Chanel* CHNL)   {curr_ch = CHNL;}

    // 给传入的Chanel注册回调函数
    void Call_register(Chanel* chanel);

    // 四个回调函数
    void Callback_read();
    void Callback_write();
    void Callback_disconn();
    void Callback_error();

    // 状态机解析报文
    void State_machine();

    // 分各种状态进行解析
    sub_state_ParseHTTP parse_requestline();                                // 解析首部
    sub_state_ParseHTTP parse_header();                                     // 解析头部

    void Set_HttpErrorMessage(int fd,int erro_num,std::string msg);         //设置错误报文

    sub_state_ParseHTTP Analyse_GetOrHead();                                //分析报文并填充发送报文
    sub_state_ParseHTTP Analyse_Post();                                     //分析报文并填充发送报文

    void Write_Response_GeneralData();                                      //填充报文通用部分，包括：版本、结果、Server和连接状态

    void Reset_Http_events(bool in);                                        //重新注册Chanel事件
    void Reset();                                                           //重置
    
};



#endif