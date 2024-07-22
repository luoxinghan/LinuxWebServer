#ifndef LIST_TIMER_H
#define LIST_TIMER_H

// 这里存储一个双向升序链表，保存定时器的时间信息

#include <stdio.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>

#include "../log/log.h"
#include "../http/http_conn.h"

// 定时器类
class Util_Timer;

struct Client_Data {
    // 保存某个客户端的数据
    sockaddr_in address;
    // 保存socket通信文件描述符
    int sockfd; // 时间到期就关闭sockfd
    // 定时器
    Util_Timer * timer; // 每个连接的客户端都有一个定时器
};

// 可以理解为，这是双向链表的一个节点
class Util_Timer {
public:
    Util_Timer() : prev(NULL), next(NULL) {}
public:
    // 超时时间
    time_t expire_time;

    // 回调函数，当计时器超时，需要调用回调函数，断开连接，并且从epoll中删除fd
    void (* cb_func)(Client_Data *);

    // 连接资源
    Client_Data * user_data;

    // 前向指针
    Util_Timer * prev;

    // 后向指针
    Util_Timer * next;
};

// 定时器链表，是一个升序的双向链表
class Sort_Timer_List {
public:
    // 构造函数
    Sort_Timer_List();
    // 析构函数
    ~Sort_Timer_List();
    
    // 添加到链表中
    bool add_timer(Util_Timer * timer);

    // 从链表中删除
    bool del_timer(Util_Timer * timer);

    // 修改链表中的Timer
    bool update_timer(Util_Timer * timer);

    // 每次调用tick，就会从list中从头取下超时的计时器
    // 因为程序会通过信号来触发tick，因此tick每次都执行然后释放那些超时的文件描述符
    void tick();
    
private:
    // 内部接口，因为始终add_timer是一个升序排列的，因此新入节点如果插入在中间的，就需要传入整个链表遍历
    bool add_timer(Util_Timer * timer, Util_Timer * list_head);

    Util_Timer * head;  // 头节点
    Util_Timer * tail;  // 指向尾节点
};

class Utils {
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数，向管道中写入信号
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    Sort_Timer_List m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};

//这是一个实际的回调函数
void cb_func(Client_Data *user_data);

#endif