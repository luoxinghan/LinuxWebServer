#include "locker.h"

Locker::Locker(){
    // 初始化互斥锁，如果互斥锁成功初始化，就返回0，否则抛出异常
    if(pthread_mutex_init(&m_mutex, NULL) != 0) {
        // 抛出异常
        throw exception(); // 直接抛出一个异常对象
    }
}

Locker::~Locker() {
    // 销毁该锁
    pthread_mutex_destroy(&m_mutex);
}

bool Locker::lock() {
    // 直接返回
    return (pthread_mutex_lock(&m_mutex) == 0);
}

bool Locker::unlock() {
    // 同样 直接返回
    return (pthread_mutex_unlock(&m_mutex) == 0);
}

pthread_mutex_t * Locker::getMutex() {
    return &m_mutex;
}

Cond::Cond() {
    if(pthread_cond_init(&m_cond, NULL) != 0) {
        throw exception(); // 直接抛出异常的最大父类
    }
}

// 析构函数
Cond::~Cond() {
    pthread_cond_destroy(&m_cond); // 直接摧毁条件变量
}

// 用于等待条件变量等待signal 
bool Cond::wait(pthread_mutex_t * mutex) {
    return (pthread_cond_wait(&m_cond, mutex) == 0); // 等待signal信号的函数
}

// 用于等待信号，并在time时间后取消阻塞，time是一个timespec结构体
bool Cond::timedwait(pthread_mutex_t * mutex, timespec time) {
    return (pthread_cond_timedwait(&m_cond, mutex, &time) == 0); // 等待signal信号的函数
}

// 发送一个信号，在wait或者timedwait接收到信号后解除阻塞
bool Cond::signal() {
    return (pthread_cond_signal(&m_cond) == 0);
}

// 广播唤醒所有的正在wait的线程
bool Cond::broadcast() {
    return (pthread_cond_broadcast(&m_cond) == 0);
}

// 默认构造函数
Semaphore::Semaphore() {
    // 初始化信号量
    if(sem_init(&m_sem, 0, 0) == -1) {
        throw exception();
    }
}

// 带参构造函数
Semaphore::Semaphore(int num) {
    // 初始化信号量
    if(sem_init(&m_sem, 0, num) == -1) {
        throw exception();
    }
}

// 析构函数
Semaphore::~Semaphore() {
    sem_destroy(&m_sem);
}

// 等待信号量（会使得内部计数器-1）
bool Semaphore::wait() {
    return (sem_wait(&m_sem) == 0);
}

// 增加信号量（会使得内部计数器+1）
bool Semaphore::post() {
    return (sem_post(&m_sem) == 0);
}

