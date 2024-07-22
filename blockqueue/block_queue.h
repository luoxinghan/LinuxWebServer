/*************************************************************
*循环数组实现的阻塞队列，m_back = (m_back + 1) % m_max_size;  
*线程安全，每个操作前都要先加互斥锁，操作完后，再解锁
**************************************************************/

#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "../locker/locker.h"

// 什么是阻塞队列，这里的作用就是存放一系列的事件（日志）
// 因为写入日志的过程，如果写入非常多，一次写会产生很大的CPU消耗，因此通过一个队列保存写入

template <class T>
class Block_Queue {
public:
    // 默认构造函数
    Block_Queue(int max_size = 10000) {
        if(max_size < 0) {
            cout << "Wrong block queue size : ": << max_size << endl;
            exit(-1);
        }
        // 初始化
        m_max_size = max_size;          // 赋值最大阻塞队列数量
        m_array = new T[m_max_size];    // 初始化数组
        m_size = 0;                     // 初始化数组大小为0
        m_front= -1;                    // 初始化front为-1
        m_back = -1;                    // 初始化back为-1
    }

    // 析构函数
    ~Block_Queue() {
        m_mutex.lock();
        // 如果m_array有数据，就释放掉内存
        if(!m_array) {
            delete [] m_array;
        }
        m_mutex.unlock();
    }

    // 添加任务到阻塞队列中
    bool push(const T &item)){
        // 先上锁，因为马上要开始操作阻塞队列了
        m_mutex.lock();
        if(m_size >= m_max_size) {
            // 里面已经有很多数据了，广播告诉阻塞的进程赶紧来pop取出去
            // 这种情况是因为，可能之前队列为空，然后有很多线程想要读取的都阻塞了。
            // 但一瞬间来了很多数据进队列给队列占满了，此时要广播告诉这些线程来取（虽然每次都广播了，但还是需要考量的）
            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }
        // 下方m_back的计算，为循环处理的，因此取余数
        m_back = (m_back + 1) % m_max_size;
        // 将对应的地址放到m_array阻塞队列中
        m_array[m_back] = item;
        // size++
        m_size++;

        m_cond.broadcast();
        m_mutex.unlock();
        return true;
    }

    // 从阻塞队列中删除任务
    bool pop(T &item) {
        m_mutex.lock();
        //多个消费者要用while 
        /*
            有可能多个线程都在等待这个资源可用的信号，信号发出后只有一个资源可用，
            但是有A，B两个线程都在等待，B比较速度快，获得互斥锁，然后加锁，消耗资源，然后解锁，
            之后A获得互斥锁，但A回去发现资源已经被使用了，
            它便有两个选择，一个是去访问不存在的资源，另一个就是继续等待，
            那么继续等待下去的条件就是使用while，要不然使用if的话pthread_cond_wait返回后，就会顺序执行下去。
        */

        // 如果size小于等于0，说明队列为空
        while (m_size <= 0) {
            // wait函数成功完成会返回0
            // 如果等待失败了，就会返回false 因为在Locker中是重写了的
            if (!m_cond.wait(m_mutex.getMutex())) {
                m_mutex.unlock();
                return false;
            }
        }

        // 下面就是删除一个任务所需要的代码，不解释了
        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }
    
    //增加了超时处理
    //在pthread_cond_wait基础上增加了等待的时间，只指定时间内能抢到互斥锁即可
    bool pop(T &item, int ms_timeout) {
        struct timespec t = {0, 0};
        struct timeval now = {0, 0};
        //其参数now是保存获取时间结果的结构体
        gettimeofday(&now, NULL);
        m_mutex.lock();
        if (m_size <= 0) {
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            t.tv_nsec = (ms_timeout % 1000) * 1000;
            if (!m_cond.timewait(m_mutex.get(), t)) {
                m_mutex.unlock();
                return false;
            }
        }

        if (m_size <= 0) {
            m_mutex.unlock();
            return false;
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }

    // 判断阻塞队列是否为空
    bool is_empty() {
        m_mutex.lock();
        if(m_size == 0) {
            m_mutex.unlock();
            return ture;
        }
        m_mutex.unlock();
        return false;
    }

    // 判断阻塞队列是否满了
    bool is_full() {
        m_mutex.lock();
        if(m_size >= m_max_size) {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    // 获取阻塞队列的大小
    int size() {
        int temp = 0;
        m_mutex.lock();
        temp = m_size;
        m_mutex.unlock();
        return temp;
    }

    // 获取阻塞队列的最大值
    int max_size() {
        int temp = 0;
        m_mutex.lock();
        temp = m_max_size;
        m_mutex.unlock();
        return temp;
    }

    // 获取阻塞队列的队列头
    // 注意是通过引用传递的
    bool front(T &value) {
        m_mutex.lock();
        if (0 == m_size) {
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_front];
        m_mutex.unlock();
        return true;
    }

    // 获取阻塞队列的队列尾
    bool back(T &value) {
        m_mutex.lock();
        if (0 == m_size) {
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_back];
        m_mutex.unlock();
        return true;
    }

private:
    Locker m_mutex;     // 互斥锁，为了操作阻塞队列
    Cond m_cond;        // 条件变量，判断是否阻塞队列有数据的
    // 如果阻塞队列为空，那么读取就会被条件变量阻塞
    // 如果阻塞队列满了，那么写入也会被条件变量阻塞


    T * m_array;        // 阻塞队列指针
    int m_size;         // 阻塞队列的数据长度
    int m_max_size;     // 阻塞队列的最大长度
    int m_front;        // 当前位置的front 始终指向对头元素的下标
    int m_back;         // 当前位置的back 始终指向队尾元素下标
};

#endif