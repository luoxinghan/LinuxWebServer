# include "config.h"

Config::Config() {
    port = 9000; // 9000 (默认)
    trig_mode = 0; // socket触发模式
    lfd_trig_mode = 0; // 监听文件描述符触发模式LT
    cfd_trig_mode = 0; // 通信文件描述符触发模式LT
    conn_thread_num = 8; // 通信线程连接数量默认8个
    sql_thread_num = 5; // 连接数据库的线程默认5个
    log_open = 1; // 默认打开日志记录
    log_write_way = 0; // 默认同步方式记录日志
    socket_linger_opt = 0; // 默认不强制close文件描述符
    actor_mode = 1; // 默认为Proactor模式
}

Config::~Config(){}

void Config::set_trig_mode() {
    switch (trig_mode) {
        case 0: {
            lfd_trig_mode = 0;
            cfd_trig_mode = 0;
            break;
        }
        case 1: {
            lfd_trig_mode = 0;
            cfd_trig_mode = 1;
            break;
        }
        case 2: {
            lfd_trig_mode = 1;
            cfd_trig_mode = 0;
            break;
        }
        case 3: {
            lfd_trig_mode = 1;
            cfd_trig_mode = 1;
            break;
        }
    
        default: {
            lfd_trig_mode = 0;
            cfd_trig_mode = 0;
            break;
        }
    }
}

void Config::parse_arg_(int argc, char * argv[]) {
    // 这里主函数会传入参数argc和argv
    // 其中argc是包含了地址的数量，即参数数量+1
    int optVal; // 选项
    const char * optStr = "p:t:c:s:o:w:l:a:";
    while((optVal == getopt(argc, argv, optStr)) != -1) {
        switch(optVal) {
            case 'p': {
                // 设置端口号
                port = atoi(optarg);
                break;
            }
            case 't': {
                // 设置socket触发模式
                trig_mode = atoi(optarg);
                set_trig_mode();
                break;
            }
            case 'c': {
                // 设置线程数量
                conn_thread_num = atoi(optarg);
                break;
            }
            case 's': {
                // 设置连接数据库线程数量
                sql_thread_num = atoi(optarg);
                break;
            }
            case 'o': {
                // 设置日志是否打开
                log_open = atoi(optarg);
                break;
            }
            case 'w': {
                // 设置日志写入方式（同步异步）
                log_write_way = atoi(optarg);
                break;
            }
            case 'l': {
                // 设置close
                socket_linger_opt = atoi(optarg);
                break;
            }
            case 'a': {
                // 设置actor模式
                actor_mode = atoi(optarg);
                break;
            }
            default:
                break;
        }
    }
}
