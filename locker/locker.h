#ifndef LOCKER_H
#define LOCKER_H

// 线程同步机制封装类
#include <pthread.h>
#include <exception>
#include <semaphore.h>
using namespace std;

// 互斥锁类
class Locker {
public:
    // 无参构造函数
    Locker();

    // 析构函数
    ~Locker();

    // 互斥锁类上锁
    bool lock();

    // 互斥锁类解锁
    bool unlock();
    
    // 返回互斥锁的地址
    pthread_mutex_t * getMutex();

private:
    // 互斥锁成员变量
    pthread_mutex_t m_mutex;
};

// 条件变量类
class Cond {
public:
    // 默认构造函数
    Cond();

    // 析构函数
    ~Cond();

    // 用于等待条件变量等待signal 
    bool wait(pthread_mutex_t * mutex);

    // 用于等待信号，并在time时间后取消阻塞，time是一个timespec结构体
    bool timedwait(pthread_mutex_t * mutex, timespec time);

    // 发送一个信号，在wait或者timedwait接收到信号后解除阻塞
    bool signal();

    // 广播唤醒所有的正在wait的线程
    bool broadcast();

private:
    // 创建一个条件变量，作为自定义类的条件变量
    pthread_cond_t m_cond;
};

// 信号量类
class Semaphore {
public:
    // 默认构造函数
    Semaphore();

    // 带参构造函数
    Semaphore(int num);

    // 析构函数
    ~Semaphore();

    // 等待信号量（会使得内部计数器-1）
    bool wait();

    // 增加信号量（会使得内部计数器+1）
    bool post();

private:
    // 创建一个信号量，作为自定义类的信号量
    sem_t m_sem;
};


#endif