#include "http_conn.h"
#include "threadpool.h"
#include "conn_timer.h"
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <signal.h>
#include <time.h>

extern conn_timer_list TIMER_LIST;
extern int pipefd[2];
#define MAX_USER_NUM 65534
#define MAX_EVENT_NUM 10000
epoll_event events[MAX_USER_NUM];
http_conn *users = new http_conn[MAX_USER_NUM];

int http_conn::m_user_num = 0;
int http_conn::m_epoll_fd = -1;

static bool timeout = false;
bool server_stop = false;
void timer_handler()
{
    TIMER_LIST.address_expired();
    alarm(TIMER_SLOT);
}
void sig_handler(int num)
{
    send(pipefd[1], (char *)&num, 1, 0);
}
/**
 * @brief 添加信号
 *
 * @param signum 信号
 * @param handler 信号处理
 */
void add_sigaction(int signum, void(handler)(int))
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(signum, &sa, NULL);
}
int main(int argc, const char *argv[])
{
    if (argc <= 1)
    {
        printf("请指定端口号\n");
        return -1;
    }
    threadpool<http_conn> *pool = NULL;

    try
    {
        pool = new threadpool<http_conn>;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        printf("线程池创建失败\n");
        return -1;
    }

    add_sigaction(SIGPIPE, SIG_IGN);

    // 申请用于监听的文件描述符
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    printf("listen_fd = %d\n", listen_fd);
    if (listen_fd == -1)
    {
        perror("listen");
        delete[] events;
        return -1;
    }

    int port = atoi(argv[1]);
    printf("port = %d\n", port);

    // 设置端口复用
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in server_addr;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    // char *sip = "127.0.0.1";
    // inet_pton(AF_INET, sip, &server_addr.sin_addr.s_addr);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    int res = bind(listen_fd, (sockaddr *)&server_addr, sizeof(server_addr));
    if (res == -1)
    {
        perror("bind");
        close(listen_fd);
        delete[] events;
        return -1;
    }

    // 监听
    listen(listen_fd, 10);

    // 创建epoll
    int epoll_fd = epoll_create(MAX_USER_NUM);
    http_conn::m_epoll_fd = epoll_fd;
    printf("epoll_fd = %d\n", epoll_fd);

    // 监听描述符不应该oneshot
    epoll_add(epoll_fd, listen_fd, false);
    // 创建管道
    res = socketpair(AF_INET, SOCK_STREAM, 0, pipefd);
    if (res == -1)
    {
        perror("socketpair");
        close(listen_fd);
        close(epoll_fd);
        delete[] events;
        return -1;
    }
    epoll_add(epoll_fd, pipefd[0], false);
    add_sigaction(SIGALRM, sig_handler);
    alarm(TIMER_SLOT);

    while (!server_stop)
    {
        int num = epoll_wait(epoll_fd, events, MAX_EVENT_NUM, -1);
        if (num == -1 && errno != EINTR)
        {
            printf("epoll failure\n");
            break;
        }

        for (int i = 0; i < num; i++)
        {
            int fd = events[i].data.fd;
            printf("请求fd = %d\n", fd);

            // 有新的连接
            if (fd == listen_fd)
            {
                sockaddr_in addr;
                socklen_t size = sizeof(addr);
                int sockfd = accept(listen_fd, (sockaddr *)&addr, &size);
                if (http_conn::m_user_num > MAX_USER_NUM)
                {
                    // 可以给客户端提示
                    printf("服务器正忙\n");
                    close(sockfd);
                    continue;
                }

                // 记录新的连接信息
                users[sockfd].init(sockfd, addr);
                conn_timer *timer = new conn_timer(&users[sockfd]);
                TIMER_LIST.append(timer);
                users[sockfd].set_timer(timer);
            }
            else if (fd == pipefd[0])
            {
                char buf[1024];
                int len = recv(pipefd[0], buf, 1024, 0);
                for (int i = 0; i < len; i++)
                {
                    if (buf[i] == SIGALRM)
                    {
                        timeout = true;
                    }
                    else if (buf[i] == SIGTERM)
                    {
                        server_stop = true;
                    }
                }
            }
            else if (events[i].events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR))
            {
                // 客户端断开或错误
                users[fd].close_conn();
            }
            else if (events[i].events & EPOLLIN)
            {

                // 检测到读事件
                if (users[fd].read())
                {
                    // 一次性读完数据
                    pool->append(users + fd);
                    conn_timer *timer = users[fd].get_timer();
                    // 默认更新15s
                    TIMER_LIST.adjust_timer(timer);
                }
                else
                {
                    // read失败
                    users[fd].close_conn();
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                // 检测到写事件
                if (users[fd].write())
                {
                    conn_timer *timer = users[fd].get_timer();
                    // 默认更新15s
                    TIMER_LIST.adjust_timer(timer);
                }
                else
                {
                    // 写失败
                    users[fd].close_conn();
                }
            }
        }
        if (timeout)
        {
            timer_handler();
            timeout = false;
        }
    }

    close(epoll_fd);
    close(listen_fd);
    delete[] events;
    delete[] users;
    delete pool;
    return 0;
}
