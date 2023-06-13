#include "conn_timer.h"
#include "http_conn.h"
static conn_timer_list TIMER_LIST;
static int pipefd[2] = {-1, -1};

extern conn_timer_list TIMER_LIST;
extern int pipefd[2];

conn_timer::conn_timer(http_conn *user_data, time_t expire_time) : m_user_data(user_data), m_expire_time(expire_time), prev(NULL), next(NULL)
{
}
conn_timer::conn_timer(const conn_timer &timer)
{
    this->m_expire_time = timer.m_expire_time;
    this->m_user_data = timer.m_user_data;
    this->next = timer.next;
    this->prev = timer.prev;
}

conn_timer::~conn_timer()
{
    prev = NULL;
    next = NULL;
    m_user_data = NULL;
}

conn_timer_list::conn_timer_list(/* args */) : head(NULL), tail(NULL), m_length(0)
{
}

conn_timer_list::~conn_timer_list()
{
    if (!this->is_empty())
    {
        conn_timer *head = this->get_head();
        while (head)
        {
            this->pop_front();
            head = this->get_head();
        }
    }
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
 * @brief 处理过期的定时器
 *
 */
void conn_timer_list::address_expired()
{
    if (TIMER_LIST.is_empty())
    {
        return;
    }
    conn_timer *head = TIMER_LIST.get_head();
    time_t now = time(NULL);
    TIMER_LIST.lock();
    while (head && head->m_expire_time < now)
    {
        // TODO 断开连接
        http_conn *user = head->m_user_data;
        TIMER_LIST.pop_front();
        head = TIMER_LIST.get_head();
        user->close_conn();
    }
    TIMER_LIST.unlock();
}
/**
 * @brief 将定时器插入到合适的位置。链表expire从小到大。
 *
 * @param timer
 */
void conn_timer_list::append(conn_timer *timer)
{
    if (timer == NULL)
    {
        return;
    }
    if (this->is_empty())
    {
        this->push_back(timer);
        return;
    }
    // timer结束时间比head早，所以timer应该作为头结点
    if (head->m_expire_time > timer->m_expire_time)
    {
        this->push_front(timer);
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
            return;
        }
        prev = next;
        next = next->next;
    }
    return;
}
void conn_timer_list::adjust_timer(conn_timer *timer, time_t new_expire)
{
    if (timer == NULL)
    {
        return;
    }
    conn_timer *cur = head;
    while (cur != NULL)
    {
        if (cur == timer)
        {
            if ((timer->prev != NULL && new_expire < timer->prev->m_expire_time) || (timer->next != NULL && new_expire > timer->next->m_expire_time))
            {
                conn_timer *new_timer = new conn_timer(*timer);
                del_timer(timer);
                append(timer);
                return;
            }
            timer->m_expire_time = new_expire;
            return;
        }
        cur = cur->next;
    }
}
void conn_timer_list::del_timer(conn_timer *timer)
{
    if (timer == NULL)
    {
        return;
    }
    if (timer == head)
    {
        this->pop_front();
        return;
    }
    if (timer == tail)
    {
        this->pop_back();
        return;
    }
    conn_timer *cur = head->next;
    while (cur != NULL)
    {
        if (cur == timer)
        {
            cur->next = timer->next;
            timer->next->prev = cur;
            delete timer;
            return;
        }
        cur = cur->next;
    }
    return;
}