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
#include "../main.h"
#include "processhttp.h"
/*初始化客户连接，清空读缓冲区*/
void http_conn::init( int epollfd, int sockfd, const sockaddr_in& client_addr )
{
	m_epollfd = epollfd;
	m_sockfd = sockfd;
	m_address = client_addr;
	memset(m_buf, '\0', BUFFER_SIZE);
	m_read_idx = 0;
}

void http_conn::process(int fd)
{
	int is_static,contentLength=0,isGet=1;
	struct stat sbuf;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char filename[MAXLINE], cgiargs[MAXLINE],httpspostdata[MAXLINE];
	rio_t rio;
	memset(buf,0,MAXLINE);

#ifdef HTTPS
	if(ishttps)
	{
		ssl=SSL_new(ssl_ctx);
		SSL_set_fd(ssl,fd);
		if(SSL_accept(ssl)==0)
		{
			ERR_print_errors_fp(stderr);
			exit(1);
		}
		SSL_read(ssl,buf,sizeof(buf));
	}
	else
#endif
	{
		/* Read request line and headers */
		Rio_readinitb(&rio, fd); // inti the struct
		Rio_readlineb(&rio, buf, MAXLINE);
	}

	sscanf(buf, "%s %s %s", method, uri, version);

	if (strcasecmp(method, "GET")!=0&&strcasecmp(method,"POST")!=0)
	{
		//todo
		clienterror(fd, method, "501", "Not Implemented",
					"The server does not implement this method");
		return;
	}

	/* Parse URI from GET request */
/*	int stop = 1;
	while (stop) {
		sleep(1);
	}*/
	is_static = parse_uri(uri, filename, cgiargs);

	if (lstat(filename, &sbuf) < 0)
	{
		//todo
		//update the error code to a fixed page
		clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
		return;
	}

	if(S_ISDIR(sbuf.st_mode)&&isShowdir)  // weather the file is dir, and weather we need to show the dir
		serve_dir(fd,filename);


	if (strcasecmp(method, "POST")==0)
		isGet=0;


	if (is_static)
	{ /* Serve static content */
#ifdef HTTPS
		if(!ishttps)
#endif
			get_requesthdrs(&rio);  /* because https already read the headers -> SSL_read()  */
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
		{
			//todo
			clienterror(fd, filename, "403", "Forbidden", "web server couldn't read the file");
			return;
		}
		serve_static(fd, filename, sbuf.st_size);
	}
	else
	{ /* Serve dynamic content */
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
		{
			//todo
			clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
			return;
		}

		if(isGet)
		{
#ifdef HTTPS
			if(!ishttps)
#endif
				get_requesthdrs(&rio);   /* because https already read headers by SSL_read() */

			get_dynamic(fd, filename, cgiargs);
		}
		else
		{
#ifdef HTTPS
			if(ishttps)
				https_getlength(buf,&contentLength);
			else
#endif
				post_requesthdrs(&rio,&contentLength);

			post_dynamic(fd, filename,contentLength,&rio);
		}
	}
}



