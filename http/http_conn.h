#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <sys/uio.h>

using namespace std;

/**
 * 工作任务类（请求类）
 * 这个类是线程主要处理的工作，保存了一个请求信息
*/
class Http_conn {
public:
    // socket共有的一个epollfd，只有一个红黑树
    static int m_epfd;
    // 所有用户连接的数量
    static int m_user_cout;
    // 读缓冲区的固定大小
    static const int READ_BUFFER_SIZE = 2048;
    // 写缓冲区的固定大小
    static const int WRITE_BUFFER_SIZE = 1024;
    // 文件名的最大长度
    static const int MAX_FILENAME = 200;

public:
    // 定义一些状态
    // HTTP请求方法，这里只支持GET
    enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT};
    
    /*
        解析客户端请求时，主状态机的状态
        CHECK_STATE_REQUESTLINE:当前正在分析请求行
        CHECK_STATE_HEADER:当前正在分析头部字段
        CHECK_STATE_CONTENT:当前正在解析请求体
    */
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
    
        
    // 从状态机的三种可能状态，即行的读取状态，分别表示
    // 1.读取到一个完整的行 2.行出错 3.行数据尚且不完整
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };

    /*
        服务器处理HTTP请求的可能结果，报文解析的结果
        NO_REQUEST          :   请求不完整，需要继续读取客户数据
        GET_REQUEST         :   表示获得了一个完成的客户请求
        BAD_REQUEST         :   表示客户请求语法错误
        NO_RESOURCE         :   表示服务器没有资源
        FORBIDDEN_REQUEST   :   表示客户对资源没有足够的访问权限
        FILE_REQUEST        :   文件请求,获取文件成功
        INTERNAL_ERROR      :   表示服务器内部错误
        CLOSED_CONNECTION   :   表示客户端已经关闭连接了
    */
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };

public:
    // 构造函数
    Http_conn();
    // 析构函数
    ~Http_conn();

public:
    // 用于处理（响应）客户端的请求（这个就是直接放这个请求要做什么事情的）
    void process(); 
    // 初始化新接收的连接
    void init(int sockfd, const sockaddr_in &addr);
    // 关闭连接
    void close_conn();
    // 非阻塞的从文件描述符缓冲区读数据
    bool read();
    // 非阻塞向文件描述符写缓冲区写数据
    bool write();
    // 解析HTTP请求
    HTTP_CODE process_read();
    // 解析具体的信息
    HTTP_CODE do_request();
    // 解析请求首行 - 解析请求分开写
    HTTP_CODE parse_request_line(char * text);
    // 解析请求头
    HTTP_CODE parse_headers(char * text);
    // 解析请求体
    HTTP_CODE parse_content(char * text);
    // 解析具体的某一行 - 从状态机 - 根据\n获取的
    LINE_STATUS parse_line();
    // 获取一行数据（因为你读到\n就不读了）
    char * get_line(){ return m_read_buf + m_start_line; }
    // 释放内存映射
    void unmap();
    // 响应HTTP请求
    bool process_write(HTTP_CODE ret);
    // 添加响应首行
    bool add_status_line(int status, const char * title);
    // 往缓冲区写入数据
    bool add_response(const char * format, ...);
    // 响应体
    bool add_content( const char* content );
    bool add_content_type();
    bool add_headers( int content_length );
    bool add_content_length( int content_length );
    bool add_linger();
    bool add_blank_line();


private:
    // HTTP连接的Socket，该请求用于通信的
    int m_sockfd;

    // socket通信地址，存放发送请求端的
    sockaddr_in m_addr;

    // 读缓冲区
    char m_read_buf[READ_BUFFER_SIZE];

    // 标识读缓冲区中读入的客户端数据的最后一个字节的下一个位置
    // 比如一次读取不完，那么第二次读取就从下一个位置开始读取
    int m_read_index;

    // 写缓冲区
    char m_write_buf[WRITE_BUFFER_SIZE];

    int m_write_index; // 写缓冲区中待发送的字节数

    // 当前正在分析的字符在读缓冲区的位置
    int m_checked_index;

    // 当前正在解析行的起始位置
    int m_start_line;

    // 请求体的长度
    long int m_content_length;

    char * m_url; // 请求目标文件的文件名
    char * m_version; // 协议版本，只支持HTTP1.1
    METHOD m_method; // 请求版本
    char * m_host; // 主机名
    bool m_linger; // http请求是否要保持连接

    char m_real_file[MAX_FILENAME]; // 请求的资源的url
    struct stat m_file_stat; // 当前文件的状态
    char * m_file_address; // 客户请求的目标文件被mmap到内存中的起始位置

    // 主状态机当前所处的状态
    CHECK_STATE m_checked_state;

    // 对其他的数据（和状态机相关的数据）进行初始化
    void init();

    struct iovec m_iv[2];                   // 我们将采用writev来执行写操作，所以定义下面两个成员，其中m_iv_count表示被写内存块的数量。
    int m_iv_count;
};

#endif