#ifndef CONFIG_H
#define CONFIG_H

#include <unistd.h>
#include <string>

// config类是Server一些可配置的参数信息得到的
// 当运行的过程中，在server后部添加一些配置信息，最终得到的config
class Config {
public:
    // 无参构造函数
    Config();

    // 析构函数
    ~Config();

public:
    // 对传入参数进行解析
    void parse_arg_(int argc, char * argv[]);

    // 根据触发模式设置监听文件描述符和通信文件描述符触发模式
    void set_trig_mode();

public:
    // 端口号
    // - 9000 (默认)
    int port;

    // 触发模式（水平模式或边沿模式）
    // - 0 : 监听LT 通信LT (默认)
    // - 1 : 监听LT 通信ET
    // - 2 : 监听ET 通信LT
    // - 3 : 监听ET 通信ET
    int trig_mode;

    // 监听文件描述符模式
    // - 0 : 水平模式 (LT)
    // - 1 : 边沿模式 (ET)
    int lfd_trig_mode;

    // 通信文件描述符模式
    // - 0 : 水平模式 (LT)
    // - 1 : 边沿模式 (ET)
    int cfd_trig_mode;

    // 通信线程池线程数量
    // 默认 = 8
    int conn_thread_num;

    // 数据库连接线程池线程数量
    // 默认 = 5
    int sql_thread_num;

    // 是否打开日志记录
    // - 0 : 打开
    // - 1 : 关闭 (默认)
    int log_open;

    // 日志写入方式
    // - 0 : 同步写入 (默认)
    // - 1 : 异步写入
    int log_write_way;

    // 是否强制close(fd)（为setsockopt的SO_LINGER的设置）
    // - 0 : 否 (默认)
    // - 1 : 是
    int socket_linger_opt;

    // 设置核反应堆的模式
    // - 0 Reactor : 主线程光接受请求，子线程读写数据，并且还要处理请求
    // - 1 Proactor : 主线程读写请求，子线程处理请求任务  (默认)
    int actor_mode;
};


#endif