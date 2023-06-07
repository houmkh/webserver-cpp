#include <arpa/inet.h>

int main(int argc, const char **argv)
{

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr.s_addr);
    connect(sockfd, (sockaddr *)&sa, sizeof(sa));
    return 0;
}