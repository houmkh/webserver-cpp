#ifndef CONN_TIMER_H
#include "http_conn.h"
#include "locker.h"
#include <signal.h>
#include <time.h>

#define CONN_TIMER_H
#define TIMER_SLOT 5
conn_timer_list TIMER_LIST;
class conn_timer
{
public:
    http_conn *m_user_data;
    time_t m_expire_time;
    conn_timer *prev;
    conn_timer *next;

public:
    conn_timer(http_conn *user_data, time_t expire_time = time(NULL) + TIMER_SLOT * 3);
    ~conn_timer();
};
conn_timer::conn_timer(http_conn *user_data, time_t expire_time) : m_user_data(user_data), m_expire_time(expire_time), prev(NULL), next(NULL)
{
}

conn_timer::~conn_timer()
{
    prev = NULL;
    next = NULL;
    m_user_data = NULL;
}

class conn_timer_list
{
private:
    conn_timer *head;
    conn_timer *tail;
    int m_length;
    locker m_locker;

public:
    conn_timer_list(/* args */);
    ~conn_timer_list();
    void append(conn_timer *timer);
    void adjust_timer(conn_timer *timer);
    void del_timer(conn_timer *timer);

    void push_back(conn_timer *timer);
    void push_front(conn_timer *timer);
    conn_timer *pop_back();
    conn_timer *pop_front();
    void insert(conn_timer *prev, conn_timer *next, conn_timer *timer);

    void add_length() { m_length++; }
    void sub_length() { m_length--; }
    bool is_empty() { return m_length == 0; }
    bool is_one_node() { return !is_empty() && head == tail; }

    conn_timer *get_head() { return head; }
    conn_timer *get_tail() { return tail; }
};

conn_timer_list::conn_timer_list(/* args */) : head(NULL), tail(NULL), m_length(0)
{
}

conn_timer_list::~conn_timer_list()
{
    head = NULL;
    tail = NULL;
}
/**
 * @brief 尾插
 *
 * @param timer
 */
void conn_timer_list::push_back(conn_timer *timer)
{
    if (this->is_empty())
    {
        timer->next = NULL;
        timer->prev = NULL;
        this->head = timer;
    }
    else
    {
        timer->prev = tail;
        tail->next = timer;
    }
    tail = timer;
    this->add_length();
}
/**
 * @brief 头插
 *
 * @param timer
 */
void conn_timer_list::push_front(conn_timer *timer)
{
    if (this->is_empty())
    {
        timer->next = NULL;
        timer->prev = NULL;
        this->tail = timer;
    }
    else
    {
        head->prev = timer;
        timer->next = head;
    }
    head = timer;
    this->add_length();
    return;
}

conn_timer *conn_timer_list::pop_back()
{
    if (!is_empty())
    {
        this->sub_length();
        conn_timer *ret = tail;
        if (is_one_node())
        {
            head = NULL;
            tail = NULL;
        }
        else
        {
            tail = tail->prev;
            tail->next = NULL;
            ret->prev = NULL;
        }
        return ret;
    }
    return NULL;
}
conn_timer *conn_timer_list::pop_front()
{
    if (!is_empty())
    {
        this->sub_length();
        conn_timer *ret = head;
        if (is_one_node())
        {
            head = NULL;
            tail = NULL;
        }
        else
        {
            head = head->next;
            head->prev = NULL;
            ret->next = NULL;
        }
        return ret;
    }
    return NULL;
}
/**
 * @brief 插入prev和next中间
 *
 * @param prev
 * @param next
 * @param timer
 */
void conn_timer_list::insert(conn_timer *prev, conn_timer *next, conn_timer *timer)
{
    if (prev != NULL)
    {
        timer->prev = prev;
        prev->next = timer;
        timer->next = next;
        if (next != NULL)
        {
            next->prev = timer;
        }
        else
        {
            tail = timer;
        }
        this->add_length();
    }
    return;
}

/**
 * @brief 将定时器插入到合适的位置。链表expire从小到大。
 *
 * @param timer
 */
void conn_timer_list::append(conn_timer *timer)
{
    this->m_locker.lock();
    if (this->is_empty())
    {
        this->push_back(timer);
        this->m_locker.unlock();
        return;
    }
    // timer结束时间比head早，所以timer应该作为头结点
    if (head->m_expire_time > timer->m_expire_time)
    {
        this->push_front(timer);
        this->m_locker.unlock();

        return;
    }
    conn_timer *prev = head, *next = head->next;
    // timer一定比链表中某一个结点晚结束
    while (prev)
    {
        if (prev->m_expire_time <= timer->m_expire_time)
        {
            if (prev == tail)
            {
                this->push_back(timer);
            }
            else
            {
                this->insert(prev, next, timer);
            }
            this->m_locker.unlock();
            return;
        }
        prev = next;
        next = next->next;
    }
    this->m_locker.unlock();
    return;
}
void conn_timer_list::adjust_timer(conn_timer *timer)
{

}
void conn_timer_list::del_timer(conn_timer *timer)
{
    
}

#endif // !CONN_TIMER_H