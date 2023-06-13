#ifndef CONN_TIMER_H
#include "http_conn.h"
#include "locker.h"
#include <signal.h>
#include <time.h>

#define CONN_TIMER_H
#define TIMER_SLOT 5


class conn_timer
{
public:
    http_conn *m_user_data;
    time_t m_expire_time;
    conn_timer *prev;
    conn_timer *next;

public:
    conn_timer(const conn_timer &timer);
    conn_timer(http_conn *user_data, time_t expire_time = time(NULL) + TIMER_SLOT * 3);
    ~conn_timer();
};

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
    void address_expired();
    void append(conn_timer *timer);
    void adjust_timer(conn_timer *timer, time_t new_expire = time(NULL) + TIMER_SLOT * 3);
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
    void lock() { this->m_locker.lock(); }
    void unlock() { this->m_locker.unlock(); }
};



#endif // !CONN_TIMER_H
