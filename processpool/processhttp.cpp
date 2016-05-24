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
#include "../wrap.h"
#include "../parse.h"
#include "../protocal.h"
#include "processhttp.h"
#include "processpool.h"

/*初始化客户连接，清空读缓冲区*/
void http_conn::init(int epollfd, int sockfd)
{
	m_epollfd = epollfd;
	m_sockfd = sockfd;
//	m_address = client_addr;
//	memset(m_buf, '\0', BUFFER_SIZE);
//	m_read_idx = 0;
}

void http_conn::process()
{
	int is_static,contentLength=0,isGet=1;
	struct stat sbuf;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char filename[MAXLINE], cgiargs[MAXLINE],httpspostdata[MAXLINE];
	rio_t rio;
	memset(buf,0,MAXLINE);
	memset(method,0,MAXLINE);
	memset(uri,0,MAXLINE);
	memset(version,0,MAXLINE);
	memset(filename,0,MAXLINE);
	memset(cgiargs,0,MAXLINE);
	memset(httpspostdata,0,MAXLINE);

	int ret;

	/* Read request line and headers */
	Rio_readinitb(&rio, m_sockfd); // init the struct
	if((ret = Rio_readlineb(&rio, buf, MAXLINE)) <= 0)
	{
		removefd(m_epollfd,m_sockfd);
		return;
	}
	else
	{
		sscanf(buf, "%s %s %s", method, uri, version);
		if (strcasecmp(method, "GET")!=0&&strcasecmp(method,"POST")!=0)
		{
			//todo
			clienterror(m_sockfd, method, "501", "Not Implemented",
						"The server does not implement this method");
			removefd(m_epollfd,m_sockfd);
			return;
		}
/*	int stop = 1;
	while (stop) {
		sleep(1);
	}*/
		is_static = parse_uri(uri, filename, cgiargs);

		if (lstat(filename, &sbuf) < 0)
		{
			//todo
			//update the error code to a fixed page
			clienterror(m_sockfd, filename, "404", "Not found", "The server couldn't find this file");
			removefd(m_epollfd,m_sockfd);
			return;
		}

		if(S_ISDIR(sbuf.st_mode)&&isShowdir)  // weather the file is dir, and weather we need to show the dir
			serve_dir(m_sockfd,filename);

		if (strcasecmp(method, "POST")==0)
			isGet=0;


		if (is_static)
		{ /* Serve static content */
			get_requesthdrs(&rio);  /* because https already read the headers -> SSL_read()  */
			if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
			{
				//todo
				clienterror(m_sockfd, filename, "403", "Forbidden", "web server couldn't read the file");
				removefd(m_epollfd,m_sockfd);
				return;
			}
			//serve_static(m_sockfd, filename, sbuf.st_size);
			int srcfd;
			char *srcp, filetype[MAXLINE], buf[MAXBUF];
			int filesize = sbuf.st_size;
			/* Send response headers to client */
			get_filetype(filename, filetype);
			sprintf(buf, "HTTP/1.0 200 OK\r\n");
			sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
			sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
			sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);

			/* Send response body to client */
			srcfd = Open(filename, O_RDONLY, 0);
			srcp =(char*) Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
			Close(srcfd);

            // send start line and the reponse header
            int ret = rio_writen(m_sockfd, buf, strlen(buf));
            if (ret!= strlen(buf))
            {
                syslog(LOG_CRIT,"Rio_writen error");
                fprintf(stderr, "%s: %s\n", "Rio_writen error", strerror(errno));
                removefd(m_epollfd,m_sockfd);
                return;

            }
            // send the reponse body
            ret = rio_writen(m_sockfd, srcp, filesize);
            if ( ret!= filesize)
            {
                syslog(LOG_CRIT,"Rio_writen error");
                fprintf(stderr, "%s: %s\n", "Rio_writen error", strerror(errno));
                removefd(m_epollfd,m_sockfd);
                return;
            }
			Munmap(srcp, filesize);
			removefd(m_epollfd,m_sockfd);
			return;
		}
		else
		{ /* Serve dynamic content */
			if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
			{
				//todo
				clienterror(m_sockfd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
				removefd(m_epollfd,m_sockfd);
				return;
			}

			if(isGet)
			{
				get_requesthdrs(&rio);   /* because https already read headers by SSL_read() */
				get_dynamic(m_sockfd, filename, cgiargs);
				removefd(m_epollfd,m_sockfd);
				return;
			}
			else
			{
				post_requesthdrs(&rio,&contentLength);
				post_dynamic(m_sockfd, filename,contentLength,&rio);
				removefd(m_epollfd,m_sockfd);
				return;
			}
		}
	}
}
int http_conn::m_epollfd = -1;


