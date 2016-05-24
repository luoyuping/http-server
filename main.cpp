#include <assert.h>
#include "wrap.h"
#include "parse.h"
#include "protocal.h"
#include "processpool/processhttp.h"
#include "processpool/processpool.h"


int main(int argc, char **argv)
{
	int listenfd,connfd, port,clientlen;
	pid_t pid;
	struct sockaddr_in clientaddr;
	char isdaemon=0,*portp=NULL,*logp=NULL,tmpcwd[MAXLINE];

#ifdef HTTPS
	int sslport;
	char dossl=0,*sslportp=NULL;
#endif

	openlog(argv[0],LOG_NDELAY|LOG_PID,LOG_DAEMON);
	cwd=(char*)get_current_dir_name();
	strcpy(tmpcwd,cwd);
	strcat(tmpcwd,"/");
	/* parse argv */

#ifdef  HTTPS
	parse_option(argc,argv,&isdaemon,&portp,&logp,&sslportp,&dossl);
	sslportp==NULL ?(sslport=atoi(Getconfig("httpsport"))) : (sslport=atoi(sslportp));
	if(dossl==1||strcmp(Getconfig("dossl"),"yes")==0)
		dossl=1;
#else
	parse_option(argc,argv,&isdaemon,&portp,&logp);
#endif

	portp==NULL ?(port=atoi(Getconfig("httpport"))) : (port=atoi(portp));
	Signal(SIGCHLD,sigChldHandler);


	/* init log */
	if(logp==NULL)
		logp=Getconfig("log");
    initlog(logp);

    /*init the http root file dir */
    memset(&http_root_dir,0,MAXLINE);
    strncpy(http_root_dir,Getconfig("root"),strlen(Getconfig("root")));

	/* whethe show dir */
	if(strcmp(Getconfig("dir"),"no")==0)
		isShowdir=0;


	clientlen = sizeof(clientaddr);


	if(isdaemon==1||strcmp(Getconfig("daemon"),"yes")==0)
		if(Daemon(1,1)  < 0)
            exit(0);

	writePid(1);

	/* $https start  */
#ifdef HTTPS
	if(dossl)
	{
		if((pid=Fork())==0)
		{
			listenfd= Open_listenfd(sslport);
            assert(listenfd > 0);
			ssl_init();

			while(1)
			{
				connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t*)&clientlen);
				if(access_ornot(inet_ntoa(clientaddr.sin_addr))==0)
				{
					clienterror(connfd,"this web server is not open to you!" , "403", "Forbidden", "The server couldn't read the file");
					continue;
				}

				if((pid=Fork())>0)
				{
					Close(connfd);
					continue;
				}
				else if(pid==0)
				{
					ishttps=1;
					doit(connfd);
					exit(1);
				}
			}
		}
	}
#endif
	listenfd = Open_listenfd(port);
    assert(listenfd > 0);
    // use the process pool to deal with the http request
    processpool<http_conn>* pool = processpool<http_conn>::create(listenfd);
    if(pool)
    {
        pool->run();
        delete pool;
    }
    close(listenfd);
    return 0;
}

