//
// Created by luo on 16-5-15.
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#ifndef LINUX_C_WEB_SERVER_PROCESSHTTP_H
#define LINUX_C_WEB_SERVER_PROCESSHTTP_H
class http_conn
{
public:
    http_conn(){}
    ~http_conn(){}
    void init( int epollfd, int sockfd, const sockaddr_in& client_addr );
    void process(int fd);
private:
    /*读缓冲区的大小*/
    static const int BUFFER_SIZE = 1024;
    static int m_epollfd;
    int m_sockfd;
    sockaddr_in m_address;
    char m_buf[ BUFFER_SIZE ];
    /*标记读缓冲区中已经读入的客户数据最后一个字节的下一个位置*/
    int m_read_idx;
};
int http_conn::m_epollfd = -1;

#endif //LINUX_C_WEB_SERVER_PROCESSHTTP_H
