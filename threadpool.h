#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "locker.h"
#include <exception>
#include <pthread.h>
#include <list>
#include <iostream>
#include <semaphore.h>

template <typename T>
class threadpool
{
public:
    threadpool(int pool_size = 8, int request_num = 10000);
    ~threadpool();
    // 将要处理的请求放入工作队列
    bool append(T *work_package);

private:
    // 池的大小（线程数量）
    int m_pool_size;
    // 请求最大数量
    int m_request_num;
    // 线程数组
    pthread_t *m_threads;
    // 工作队列锁
    locker m_queue_locker;
    // 工作队列
    std::list<T *> m_work_queue;
    // 判断队列是否有工作需要处理
    sem m_queue_stat;
    // 线程是否继续工作
    bool m_is_stop;

private:
    // 线程工作函数
    static void *worker(void *arg);
    void run();
};

template <typename T>
threadpool<T>::threadpool(int pool_size, int request_num) : m_pool_size(pool_size), m_request_num(request_num), m_threads(NULL), m_is_stop(false)
{
    if (pool_size <= 0 || request_num <= 0)
    {
        throw std::exception();
    }
    // 初始化线程
    m_threads = new pthread_t[pool_size];
    if (m_threads == NULL)
    {
        throw std::exception();
    }

    pthread_attr_t threads_attr;
    pthread_attr_init(&threads_attr);
    pthread_attr_setdetachstate(&threads_attr, PTHREAD_CREATE_DETACHED);
    for (int i = 0; i < pool_size; i++)
    {
        if (pthread_create(m_threads + i, &threads_attr, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        printf("thread %i is ready\n", i);
    }
}

template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_is_stop = true;
}

template <typename T>
bool threadpool<T>::append(T *work_package)
{
    printf("enter append\n");
    if (m_work_queue.size() >= m_request_num)
    {
        std::cout << "work queue is full\n";
        return false;
    }
    m_queue_locker.lock();
    m_work_queue.push_back(work_package);
    m_queue_locker.unlock();
    m_queue_stat.post();
    printf("exit append\n");

    return true;
}
template <typename T>
void *threadpool<T>::worker(void *arg)
{
    // arg是一个线程池指针
    threadpool *pool = (threadpool *)arg;
    if (pool == NULL)
    {
        throw std::exception();
    }

    pool->run();
    return pool;
}
template <typename T>
void threadpool<T>::run()
{
    while (!m_is_stop)
    {
        m_queue_stat.wait();
        m_queue_locker.lock();
        if (m_work_queue.empty())
        {
            m_queue_stat.post();
            m_queue_locker.unlock();
            continue;
        }

        T *request = m_work_queue.front();
        m_work_queue.pop_front();
        m_queue_locker.unlock();
        if (request == NULL)
        {
            continue;
        }

        // 执行请求需要做的操作
        request->process();
    }
}

#endif