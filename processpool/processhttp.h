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
    void init(int epollfd, int sockfd);
    void process();
private:
    static int m_epollfd;
    int m_sockfd;
};
//int http_conn::m_epollfd = -1;

#endif //LINUX_C_WEB_SERVER_PROCESSHTTP_H
