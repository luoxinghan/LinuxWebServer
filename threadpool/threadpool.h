// 避免重复定义。这种技术被称为头文件的防御式声明（Header Guard）或者头文件的包含保护。
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <exception>
#include <list>
#include <cstdio>
#include "../locker/locker.h"
using namespace std;

/**
 * 线程池类，保存了一定数量的线程，用于处理业务
 * 为了让任务更加通用，采用模板的形式定义线程池，为了使得任务类型可以通用
*/
template <class T>
class ThreadPool {
public:
    // 初始化线程池的默认构造函数
    ThreadPool(int thread_number = 8, int max_request = 10000);

    // 析构函数
    ~ThreadPool();

    // 添加任务（请求）到线程池的请求队列中
    bool addRequest(T * request);

private:
    // 每隔线程池的业务处理函数
    static void * worker(void * arg);

    // 线程池取数据的函数
    void run();

private:
    // 定义线程属性
    // 线程的数量
    int m_thread_number;

    // 线程池数组
    pthread_t * m_threads;

    // 请求队列中的最大数量
    int m_max_request;

    // 请求队列
    list<T *> m_work_queue;

    // 互斥锁
    Locker m_queue_locker;

    // 信号量，用来判断是否有任务需要处理
    Semaphore m_queue_stat;

    // 是否结束线程
    bool m_stop;
};

template <typename T>
ThreadPool<T>::ThreadPool(int thread_number, int max_request) : 
    m_thread_number(thread_number), m_max_request(max_request), m_threads(NULL) {
        
    // 如果传入的数据都不正确，直接抛出异常
    if(thread_number <= 0 || max_request <= 0) {
        throw exception();
    }

    // 初始化m_thread_number大小的thread
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads) {
        throw exception();
    }

    // 创建m_thread_number个线程，并将它们设置为线程分离（销毁了自己释放资源）
    for(int i = 0; i < m_thread_number; i++) {
        printf("create the %d thread now...\n", i);
        // 这里就是巧妙点，在创建线程时，直接利用m_threads + i创建到数组中
        // 属性设置为NULL
        // worker必须为静态方法（反正必须在全局区）
        // 参数现在就传递NULL
        if(pthread_create(m_threads + i, NULL, worker, this) != 0) {
            delete [] m_threads; // 创建失败了，会把线程池的线程都释放掉，再抛出异常
            throw exception();
        }

        // 线程分离
        if(pthread_detach(m_threads[i])) {
            delete [] m_threads; // 创建失败了，会把线程池的线程都释放掉，再抛出异常
            throw exception();
        }
    }
}

template <typename T>
ThreadPool<T>::~ThreadPool() {
    delete [] m_threads; // 释放线程池资源
    m_stop = true; // 设置线程池结束，否则就会循环执行代码（后续代码会体现）
}

template <typename T>
bool ThreadPool<T>::addRequest(T * request) {
    // 目的是向队列中添加数据（但要保证线程同步的）
    m_queue_locker.lock(); // 上锁
    
    if(m_work_queue.size() > m_max_request) {
        // 如果任务队列已经不能放了，就解锁返回
        m_queue_locker.unlock();
        return false;
    }
    // 否则向任务队列里添加一个任务
    m_work_queue.push_back(request);
    m_queue_locker.unlock(); // 解锁
    // 信号量要增加，因为后续取数据的时候，需要根据信号量判断是否阻塞
    m_queue_stat.post();
    return true;
}

template <typename T>
void ThreadPool<T>::run() {
    // 线程池的运行函数
    // 线程池的线程需要一直循环，直到m_stop为false的时候停止
    // ***相当于主线程或者就是为了判断是否执行结束的
    while(!m_stop) {
        // 1.从任务队列取一个任务
        // 这里通过信号量来判断一下
        m_queue_stat.wait();
        // 2.上锁
        m_queue_locker.lock();
        // 3.操作队列
        if(m_work_queue.empty()){
            // 如果说队列都没有数据了，就不用做了
            m_queue_locker.unlock();
            continue;
        }
        T * request = m_work_queue.front(); // 当前子线程从前面获取一个任务
        m_work_queue.pop_front(); // 删除最前面一个
        // 4.解锁(可以和5交换)
        m_queue_locker.unlock();
        // 5.判断是否获取到请求
        if(!request) {
            continue;
        }
        // 6.当前子线程执行任务函数
        request->process();
    }
}

template <typename T>
void * ThreadPool<T>::worker(void * arg) {
    // this通过参数传递到worker中（因为这个worker是一个静态函数，静态函数无法取成员属性的值），现在解析出线程池
    ThreadPool * pool = (ThreadPool *)arg;
    pool->run(); // 直接开跑线程池
    return pool;
}


#endif