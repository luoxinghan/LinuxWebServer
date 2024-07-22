#include "list_timer.h"

Sort_Timer_List::Sort_Timer_List() {
    head = NULL;
    tail = NULL;
}

Sort_Timer_List::~Sort_Timer_List() {
    // 遍历双向链表以销毁数据
    Util_Timer * tmp = head;
    while(tmp) {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

bool Sort_Timer_List::add_timer(Util_Timer * timer) {
    // 如果连数据都没有，直接返回NULL
    if(!timer) {
        return false;
    }
    // 如果我是第一个节点
    if(!head) {
        head = tail = timer;
        return true;
    }
    // 如果目标定时器超时时间小于链表中所有定时器超时时间
    // 因为是升序，遇到一个更小的就应该
    if(timer->expire_time < head->expire_time) {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return true;
    }
    // 否则说明，当前要插入的定时器的超时时间在中间或最后
    return add_timer(timer, head);
}

bool Sort_Timer_List::add_timer(Util_Timer * timer, Util_Timer * head) {
    if(!timer) {
        return false;
    }

    // 保存一下前驱（tmp的前驱，每次tmp作为当前遍历的节点，最后就是tmp和timer比较）
    Util_Timer * prev = head;
    Util_Timer * tmp = prev->next;
    while(tmp) {
        if(timer->expire_time < tmp->expire_time) {
            prev->next = timer; // -> timer --
            timer->next = tmp; // -> timer ->
            timer->prev = prev;
            tmp->prev = timer;
            break;
        }
    }
    // 直到最后都没有找到合适位置插入，就放入到尾部
    if(!tmp) {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
        return true;
    }
    return true;
}

bool Sort_Timer_List::del_timer(Util_Timer * timer) {
    if(!timer) {
        return false;
    }
    // 如果只有一个定时器（即链表只有一个）
    if((timer == head) && (timer == tail)) {
        delete timer;
        head = tail = NULL;
        return true;
    }
    // 如果要删除的是链表表头
    if(timer == head) {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return true;
    }
    // 删除的是链表尾
    if(timer == tail) {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return true;
    }
    // 都不是
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
    return true;
}

bool Sort_Timer_List::update_timer(Util_Timer * timer) {
    if(!timer) {
        return false;
    }
    Util_Timer * tmp = timer->next; // 把timer的next保存，以它为head头节点往后新增
    // tmp为NULL说明是尾部，后者说明超时时间改变后仍然小于后一个节点
    if(!tmp || (timer->expire_time < tmp->expire_time)) {
        return true;
    }
    // 如果目标节点是头节点
    if(timer == head) {
        // 头节点往后移动
        head = head->next;
        head->prev = NULL;
        
        timer->next = NULL;
        // 头节点，head可以放tmp的其实
        add_timer(timer, head);
    } else {
        // 首先断开该节点
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

//SIGALARM信号每次触发就会在其信号处理函数中执行一次tick函数，以处理链表上的到期任务
void Sort_Timer_List::tick() {
    //链表为空，那么就不处理
    if (!head) {
        return;
    }
    //获取系统当前时间
    time_t cur = time(NULL);
    //头节点
    Util_Timer *tmp = head;
    while (tmp) {
        //如果当前时间，小于到期时间，说明还没到期，无需处理
        if (cur < tmp->expire_time) {
            break;
        }
        //调用定时器的回调函数，执行定时任务（从epoll上删除，关闭fd）
        tmp->cb_func(tmp->user_data);
        //更新新的头，往后走
        head = tmp->next;
        if (head) {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}

/**
 * 通过前向声明类Utils，回调函数cb_func可以引用Utils的静态成员变量或函数，
 * 而无需包含整个Utils类的头文件。这有助于减少需要编译的代码量，
 * 并且还可以避免头文件之间的循环依赖。
 * 在下面使用了utils中的epoll_fd
*/
class Utils;
void cb_func(Client_Data * user_data) {
    //删除非活动连接在socket上的注册事件
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    //关闭文件描述符
    close(user_data->sockfd);
    http_conn::m_user_cout--;
}

void Utils::init(int timeslot) { m_TIMESLOT = timeslot; }

// 对文件描述符设置非阻塞
int Utils::setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        // 读事件 | 边沿触发 | 异常断开会处理
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;
    // 比如一个线程在读取完某个socket上的数据后，开始处理这些数据，而在这些数据的处理过程中，该socket上又有新数据可读（EPOLLIN再次触发）
    // 此时另外一个线程被唤醒来读取这些新的数据，于是就出现了两个线程同时操作一个socket的局面。
    // 一个socket连接，任意一个时刻，只能被一个线程处理
    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 信号处理函数
void Utils::sig_handler(int sig) {
    // 为保证函数的可重入性，保留原来的errno
    // 可重入性表示中断后再次进入该函数，环境变量与之前相同，不会丢失数据
    int save_errno = errno;
    int msg = sig;
    // 往管道中写入信号
    // 将信号值从管道写端写入，传输字符类型，而非整型
    send(u_pipefd[1], (char *)&msg, 1, 0);
    // 将原来的errno赋值为当前的errno
    errno = save_errno;
}

// 设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart) {
    // 创建sigaction结构体变量
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    // sa_handler是一个函数指针，指向信号处理函数
    // 信号处理函数中仅仅发送信号值，不做对应逻辑处理
    sa.sa_handler = handler;
    if (restart)
        // sa_flags用于指定信号处理的行为
        // 使被信号打断的系统调用自动重新发起
        sa.sa_flags |= SA_RESTART;

    // sa_mask用来指定在信号处理函数执行期间需要被屏蔽的信号
    // sigfillset用来将参数set信号集初始化，然后把所有的信号加入到此信号集里。
    sigfillset(&sa.sa_mask);
    // sigaction
    // int sigaction(int signum, const struct sigaction *act, struct sigaction
    // *oldact); signum表示操作的信号。 act表示对信号设置新的处理方式。
    // oldact表示信号原来的处理方式
    // 返回值：0 表示成功，-1 表示有错误发生。
    // 执行sigaction函数
    // 修改信号处理动作

    // 注册信号
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler() {
    m_timer_lst.tick();
    // 设置信号传送闹钟，即用来设置信号SIGALRM在经过参数seconds秒数后发送给目前的进程。
    // 如果未设置信号SIGALRM的处理函数，那么alarm()默认处理终止进程.
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info) {
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

// static成员，必须在类外初始化
int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;