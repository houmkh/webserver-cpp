#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <exception>
#include <semaphore.h>
// 线程锁类
class locker
{
private:
    pthread_mutex_t m_mutex;

public:
    locker(pthread_mutexattr_t *mutex_attr);
    ~locker();
    // 上锁
    bool lock();
    // 解锁
    bool unlock();
    // 获取锁
    pthread_mutex_t *get_lock();
};

locker::locker()
{
    if (pthread_mutex_init(&m_mutex, NULL) != 0)
    {
        throw std::exception();
    }
}

locker::~locker()
{
    pthread_mutex_destroy(&m_mutex);
}

bool locker::lock()
{
    return pthread_mutex_lock(&m_mutex) == 0;
}

bool locker::unlock()
{
    return pthread_mutex_unlock(&m_mutex) == 0;
}

pthread_mutex_t *locker::get_lock()
{
    return &m_mutex;
}
// 条件变量类
class cond
{
private:
    pthread_cond_t m_cond;

public:
    cond();
    ~cond();
    bool wait(pthread_mutex_t *mutex);
    bool timewait(pthread_mutex_t *mutex, timespec tmspc);
    bool signal();
    bool broadcast();
}

cond::cond()
{
    if (pthread_cond_init(&m_cond, NULL))
    {
        throw std::exception();
    }
}

cond::~cond()
{
    pthread_cond_destroy(&m_cond);
}

bool cond::wait(pthread_mutex_t *mutex)
{
    return pthread_cond_wait(&m_cond, mutex) == 0;
}

bool cond::timewait(pthread_mutex_t *mutex, timespec tmspc)
{
    return pthread_cond_timedwait(&m_cond, mutex, &tmspc) == 0;
}

// 信号量类
class sem
{
private:
    sem_t m_sem;

public:
    sem();
    sem(int num);
    ~sem();
    // 等待信号量
    bool wait();
    // 增加信号量
    bool post();

}

sem::sem()
{
    if (sem_init(&m_sem, 0, 0))
    {
        throw std::exception();
    }
}

sem::sem(int num)
{
    if (sem_init(&m_sem, 0, num))
    {
        throw std::exception();
    }
}
sem::~sem()
{
    sem_destroy(&m_sem);
}
// 等待信号量
bool sem::wait()
{
    sem_wait(&m_sem);
}
// 增加信号量
bool sem::post()
{
    sem_post(&m_sem);
}
#endif