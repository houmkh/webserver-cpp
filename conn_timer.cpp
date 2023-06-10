#include "conn_timer.h"
# include "http_conn.h"
static conn_timer_list TIMER_LIST;
extern "C"
{
    static conn_timer_list TIMER_LIST;
}
/**
 * @brief 处理过期的定时器
 * 
 */
void address_expired()
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
    }
    TIMER_LIST.unlock();
}