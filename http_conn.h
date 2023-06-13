#ifndef HTTPCONNECTION_H

#define HTTPCONNECTION_H
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string>
#include <string.h>
#include <regex>
#include <map>
#include <unordered_map>
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <sys/mman.h>
#include <iconv.h>
#include <sys/uio.h>
#define READ_BUFFER_SIZE 2048
#define WRITE_BUFFER_SIZE 1024
class conn_timer;
/// @brief 项目根目录
const std::string ROOT_PATH = "/home/mkh/桌面/webserver-front/src";
/// @brief HTTP 状态码
const std::map<std::string, std::string> HTTP_STATUS_CODE = {
    // 请求成功
    {"200", "OK"},
    // 客户端请求的语法错误，服务器无法理解
    {"400", "Bad Request"},
    // 服务器理解请求客户端的请求，但是拒绝执行此请求
    {"403", "Forbidden"},
    // 服务器无法根据客户端的请求找到资源（网页）
    {"404", "Not Found"},
    // 服务器内部错误，无法完成请求
    {"500", "Internal Server Error"}

};

/// @brief 服务器处理HTTP请求的结果
enum HTTP_CODE
{
    // 请求不完整，需要继续读取数据
    NO_REQUEST = 0,
    // 获取到了完整的请求
    GET_REQUEST = 200,
    // 客户端请求语法错误
    BAD_REQUEST = 400,
    // 服务器没有客户端所需要的资源
    NO_RESOURCE = 404,
    // 该客户对资源没有足够的访问权限
    FORBIDDEN_REQUEST = 403,
    // 文件请求
    FILE_REQUEST = 200,
    // 服务器内部错误
    INTERNAL_ERROR = 500,
    // 客户端已关闭连接
    CLOSED_CONNECTION = 2,
};

/// @brief 主状态机的状态
enum CHECK_STATE
{
    // 正在分析请求首行
    CHECK_STATE_REQUESTLINE,
    // 正在分析请求头
    CHECK_STATE_HEADER,
    // 正在分析请求体
    CHECK_STATE_CONTENT,
};

/// @brief 从状态机的状态
enum LINE_STATE
{
    // 读取到完整的行
    LINE_OK = 0,
    // 行出错
    LINE_BAD,
    // 行数据不完整
    LINE_OPEN,
};

/// @brief HTTP请求方法，暂时只支持GET
enum METHOD
{
    GET = 0,
    POST,
    HEAD,
    PUT,
    DELETE,
    TRACE,
    OPTIONS,
    PATCH
};
class http_conn
{
public:
    static int m_epoll_fd;
    static int m_user_num;

    void process(); // 线程用来处理http请求的函数
    bool read();    // 读数据
    bool write();   // 写数据
    void init(int sockfd, struct sockaddr_in sockaddr);
    void close_conn();

    HTTP_CODE process_read();                 // 解析HTTP请求
    bool process_write(HTTP_CODE http_code);  // 生成HTTP响应
    HTTP_CODE parse_request_line(char *text); // 解析HTTP请求首行
    HTTP_CODE parse_header(char *text);       // 解析HTTP请求头
    HTTP_CODE parse_content(char *text);      // 解析HTTP请求体

    LINE_STATE parse_line(); // 解析一行数据(从状态机)

    HTTP_CODE do_request();
    void unmap();
    void set_timer(conn_timer *timer);
    conn_timer *get_timer();

private:
    int m_sockfd;
    sockaddr_in m_sockaddr;
    char m_read_buf[READ_BUFFER_SIZE];
    int m_read_index; // 下次读取客户端数据的起始下标
    char m_write_buf[WRITE_BUFFER_SIZE];

    int m_checked_index;       // 当前检查的字符位置
    int m_start_line;          // 当前行的起始位置
    CHECK_STATE m_check_state; // 主状态机的状态

    std::string m_url;                            // 请求的文件
    std::string m_version;                        // HTTP版本
    METHOD m_method;                              // 请求的方法
    bool m_linger;                                // 是否要保持连接
    std::map<std::string, std::string> m_headers; // 请求头
    int m_content_length;
    std::string m_content;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    char *m_file_addr;
    std::string m_response;
    void init(); // 初始化其他信息

    char *get_line() { return m_read_buf + m_start_line; };
    conn_timer *m_timer;
};

#endif // !HTTPCONNECTION_H

void epoll_add(int epoll_fd, int sock_fd, bool one_shot);
void epoll_remove(int epoll_fd, int sock_fd);
void epoll_modify(int epoll_fd, int sock_fd, int ev);
void close_connection();
