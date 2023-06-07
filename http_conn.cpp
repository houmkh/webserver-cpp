#include "http_conn.h"
/**
 * @brief 设置文件描述符非阻塞
 *
 * @param fd 文件描述符
 */
void setnonblocking(int fd)
{
    int flag = fcntl(fd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flag);
}
/**
 * @brief 添加fd到epoll
 *
 * @param epoll_fd
 * @param sock_fd
 */
void epoll_add(int epoll_fd, int sock_fd, bool one_shot)
{
    struct epoll_event epev;
    epev.data.fd = sock_fd;
    epev.events = EPOLLHUP | EPOLLIN;

    if (one_shot)
    {

        epev.events |= EPOLLONESHOT;
    }

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &epev);
    setnonblocking(sock_fd);
}

/**
 * @brief 将fd从epoll中移除
 *
 * @param epoll_fd
 * @param sock_fd
 */
void epoll_remove(int epoll_fd, int sock_fd)
{
    epoll_ctl(sock_fd, EPOLL_CTL_DEL, sock_fd, NULL);
    close(sock_fd);
}

/**
 * @brief 修改epoll
 *
 * @param epoll_fd
 * @param sock_fd
 * @param epev
 */
void epoll_modify(int epoll_fd, int sock_fd, int ev)
{
    epoll_event epev;
    epev.data.fd = sock_fd;
    epev.events = EPOLLONESHOT | ev | EPOLLRDHUP;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sock_fd, &epev);
}
/**
 * @brief
 *
 * @param sockfd
 * @param sockaddr
 */
void http_conn::init(int sockfd, struct sockaddr_in sockaddr)
{
    this->m_sockaddr = sockaddr;
    this->m_sockfd = sockfd;
    http_conn::m_user_num++;

    // 设置端口复用
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    // 将新的连接放到epoll里面
    epoll_add(m_epoll_fd, sockfd, true);
    init();
}

/**
 * @brief 初始化其余信息
 *
 */
void http_conn::init()
{
    // printf("%s : line = %d\n", __FUNCTION__, __LINE__);
    m_check_state = CHECK_STATE::CHECK_STATE_REQUESTLINE;
    m_checked_index = 0;
    m_start_line = 0;
    m_read_index = 0;
    bzero(m_read_buf, READ_BUFFER_SIZE);
    bzero(m_write_buf, WRITE_BUFFER_SIZE);
    m_url = "";
    m_version = "";
    m_response = "";
    m_content = "";
    m_content_length = 0;
    m_method = METHOD::GET;
    m_linger = false;
    m_iv_count = 0;
    // printf("%s : line = %d\n", __FUNCTION__, __LINE__);
}

void http_conn::close_conn()
{
    epoll_remove(http_conn::m_epoll_fd, this->m_sockfd);
    // close(this->m_sockfd);
    m_sockfd = -1;
    http_conn::m_user_num--;
}
// 读数据
bool http_conn::read()
{
    // printf("%s : line = %d\n", __FUNCTION__, __LINE__);
    if (m_read_index >= READ_BUFFER_SIZE)
    {
        return false;
    }
    while (1)
    {
        int len;

        len = recv(m_sockfd, m_read_buf + m_read_index, READ_BUFFER_SIZE - m_read_index, 0);
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // 读完数据了
            break;
        }
        else if (len == 0)
        {
            // 对方关闭连接
            return false;
        }
        m_read_index += len;
    }
    // printf("%s : line = %d\n", __FUNCTION__, __LINE__);

    // printf("recv data:\n%s\n", m_read_buf);

    return true;
}
// 写数据
bool http_conn::write()
{
    int temp = 0;
    int send_len = 0, len = m_response.length();
    if (len == 0)
    {
        // 没有要写回的数据
        epoll_modify(m_epoll_fd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while (true)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if (temp == -1)
        {
            if (errno == EAGAIN)
            {
                epoll_modify(m_epoll_fd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        send_len += temp;
        if (send_len >= len)
        {
            unmap();
            epoll_modify(m_epoll_fd, m_sockfd, EPOLLIN);
            if (m_linger)
            {
                init();
                return true;
            }
            return false;
        }
    }
}

/**
 * @brief 处理HTTP请求的入口函数，由线程池中的工作线程调用
 *
 */
void http_conn::process()
{
    printf("解析http数据包\n");
    // 解析HTTP请求
    HTTP_CODE ret = process_read();
    if (ret == HTTP_CODE::NO_REQUEST)
    {
        epoll_modify(m_epoll_fd, m_sockfd, EPOLLIN);
        return;
    }
    if (!process_write(ret))
    {
        close_conn();
    }
    epoll_modify(m_epoll_fd, m_sockfd, EPOLLOUT);
}

/**
 * @brief 解析HTTP请求,主状态机
 *
 * @return HTTP_CODE
 */
HTTP_CODE http_conn::process_read()
{
    LINE_STATE line_state = LINE_STATE::LINE_OK;
    HTTP_CODE ret = HTTP_CODE::NO_REQUEST;
    char *text = {0};
    while ((line_state = parse_line()) == LINE_OK ||
           (m_check_state == CHECK_STATE_CONTENT && line_state == LINE_OK))
    {

        text = get_line();
        m_start_line = m_checked_index;
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            if (ret == HTTP_CODE::BAD_REQUEST)
            {
                return ret;
            }

            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = parse_header(text);
            if (ret == HTTP_CODE::BAD_REQUEST)
            {
                return ret;
            }
            else if (ret == HTTP_CODE::GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
            {
                return do_request();
            }
            line_state = LINE_OPEN;
            break;
        }
        }
    }
    return INTERNAL_ERROR;
}

bool http_conn::process_write(HTTP_CODE http_code)
{

    std::string code = std::to_string(http_code);
    std::map<std::string, std::string>::const_iterator it = HTTP_STATUS_CODE.find(code);
    // 生成响应
    if (it != HTTP_STATUS_CODE.end())
    {
        m_response += m_version + " " + code + it->second;
        if (http_code == HTTP_CODE::FILE_REQUEST)
        {
            m_iv[0].iov_base = m_response.data();
            m_iv[0].iov_len = m_response.length() + 1;
            m_iv[1].iov_base = m_file_addr;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
        }
        return true;
    }
    return false;
}

/**
 * @brief  解析HTTP请求首行(从状态机)，获取请求方法、目标URL、HTTP版本
 *
 * @param text
 * @return HTTP_CODE
 */
HTTP_CODE http_conn::parse_request_line(char *text)
{

    std::regex pattern("([A-Z]*)\\s+(.*)\\s+(.*)");
    std::string t = text;
    std::smatch match;
    std::regex_search(t, match, pattern);
    if (match[1] == "GET")
    {
        m_method = METHOD::GET;
    }
    else
    {
        return BAD_REQUEST;
    }
    m_url = match[2].str();
    m_version = match[3].str();
    printf("method: %s,url: %s,version: %s\n", match[1].str().c_str(), m_url.c_str(), m_version.c_str());
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

/**
 * @brief 解析HTTP请求头
 *
 * @param text
 * @return HTTP_CODE
 */
HTTP_CODE http_conn::parse_header(char *text)
{

    std::string tx = text;
    std::regex pattern("(.*):(.*)\\r\\n");
    std::smatch match;
    std::string::const_iterator citer = tx.cbegin();
    // 匹配键值对，并将键值对存到m_headers里面
    while (std::regex_search(citer, tx.cend(), match, pattern))
    {
        m_headers[match[1]] = match[2];
        text += match[1].length() + match[2].length() + 4;
        tx = text;
        citer = tx.cbegin();
    }
    if (m_headers["Connection"] == "keep-alive")
    {
        m_linger = true;
    }
    if (m_headers.find("Content-Length") != m_headers.end())
    {
        text += 4;
        // 有请求体
        m_content_length = std::stoi(m_headers["Content-Length"]);
        m_check_state = CHECK_STATE_CONTENT;
        return HTTP_CODE::NO_REQUEST;
    }
    return HTTP_CODE::GET_REQUEST;
}

/**
 * @brief 解析HTTP请求体
 *
 * @param text
 * @return HTTP_CODE
 */
HTTP_CODE http_conn::parse_content(char *text)
{
    if (*(text + m_content_length) == '\0')
    {
        m_content = text;
        return HTTP_CODE::GET_REQUEST;
    }
    return NO_REQUEST;
}

/**
 * @brief  解析一行数据(从状态机)
 *
 * @return LINE_STATE
 */
LINE_STATE http_conn::parse_line()
{

    char temp;
    while (m_checked_index < m_read_index)
    {
        temp = m_read_buf[m_checked_index];
        if (temp == '\r')
        {
            if (m_checked_index + 1 == m_read_index)
            {
                return LINE_OPEN;
            }
            else if (m_read_buf[m_checked_index + 1] == '\n')
            {
                m_read_buf[m_checked_index++] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
        }
        else if (temp == '\n')
        {
            if (m_checked_index > 1 && m_read_buf[m_checked_index - 1] == '\r')
            {
                m_read_buf[m_checked_index - 1] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        m_checked_index++;
    }
    return LINE_STATE::LINE_OK;
}

HTTP_CODE http_conn::do_request()
{
    // printf("%d %s %s\n", m_method, m_url.c_str(), m_version.c_str());
    // for (std::map<std::string, std::string>::iterator i = m_headers.begin(); i != m_headers.end(); i++)
    // {
    //     std::cout << i->first << ":" << i->second << std::endl;
    // }
    // printf("%s\n", m_content.c_str());
    std::string path = ROOT_PATH + m_url;
    if (stat(path.c_str(), &m_file_stat) < 0)
    {
        return HTTP_CODE::NO_RESOURCE;
    }
    if (!(m_file_stat.st_mode & S_IROTH))
    {
        return HTTP_CODE::FORBIDDEN_REQUEST;
    }
    if (S_ISDIR(m_file_stat.st_mode))
    {
        return HTTP_CODE::BAD_REQUEST;
    }
    int fd = open(path.c_str(), O_RDONLY);
    m_file_addr = (char *)mmap(NULL, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return HTTP_CODE::FILE_REQUEST;
}

void http_conn::unmap()
{
    if (m_file_addr != NULL)
    {
        munmap(m_file_addr, m_file_stat.st_size);
    }
}
