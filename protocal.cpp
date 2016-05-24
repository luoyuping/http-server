#include <assert.h>
#include "wrap.h"
#include "parse.h"
#include "protocal.h"
#include "processpool/processhttp.h"
#include "processpool/processpool.h"

#define PID_FILE  "pid.file"

int isShowdir=1;
char* cwd;

#ifdef  HTTPS
SSL_CTX* ssl_ctx;
SSL* ssl;
char* certfile;
int ishttps=0;
char httpspostdata[MAXLINE];
char http_root_dir[MAXLINE];
#endif

/*$sigChldHandler to protect zimble process */
void sigChldHandler(int signo)
{
    Waitpid(-1,NULL,WNOHANG);
    return;
}
/*$end sigChldHandler */


#ifdef HTTPS
void ssl_init(void)
{
//	static char crypto[]="RC4-MD5";
    static char crypto[]="AESGCM";
    certfile=Getconfig("ca");

    SSL_load_error_strings();
    SSLeay_add_ssl_algorithms();
    ssl_ctx = SSL_CTX_new( SSLv23_server_method() );

    if ( certfile[0] != '\0' )
    if ( SSL_CTX_use_certificate_file( ssl_ctx, certfile, SSL_FILETYPE_PEM ) == 0 ||
         SSL_CTX_use_PrivateKey_file( ssl_ctx, certfile, SSL_FILETYPE_PEM ) == 0 || SSL_CTX_check_private_key( ssl_ctx ) == 0 )
    {
        ERR_print_errors_fp( stderr );
        exit( 1 );
    }
    if ( crypto != (char*) 0 )
    {

        if ( SSL_CTX_set_cipher_list( ssl_ctx, crypto ) == 0 )
        {
            ERR_print_errors_fp( stderr );
            exit( 1 );
        }
    }

}
#endif


/*
 * doit - handle one HTTP request/response transaction
 */
void doit(int fd)
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

//    printf("is_static:%d\n",is_static);
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
            clienterror(fd, filename, "403", "Forbidden", "web server couldn't run the CGI program");
            return;
        }
        if(isGet)
        {
#ifdef HTTPS
//            printf("in isGet \n");
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

//            printf("in isPost \n");
            post_dynamic(fd, filename,contentLength,&rio);
        }
    }
}

#ifdef HTTPS
void https_getlength(char* buf,int* length)
{
    char *p,line[MAXLINE];
    char *tmpbuf=buf;
    int lengthfind=0;

    while(*tmpbuf!='\0')
    {
        p=line;
        while(*tmpbuf!='\n' && *tmpbuf!='\0')
            *p++=*tmpbuf++;
        *p='\0';
        if(!lengthfind)
        {
            if(strncasecmp(line,"Content-Length:",15)==0)
            {
                p=&line[15];
                p+=strspn(p," \t");
                *length=atoi(p);
                lengthfind=1;
            }
        }

        if(strncasecmp(line,"\r",1)==0)
        {
            strcpy(httpspostdata,++tmpbuf);
            break;
        }
        ++tmpbuf;
    }
    return;
}
#endif
/* $end https_getlength  */

/*
 * read_requesthdrs - read and parse HTTP request headers
 */
/* $begin get_requesthdrs */
void get_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    writetime();  /* write access time in log file */
    while(strcmp(buf, "\r\n"))
    {
        Rio_readlineb(rp, buf, MAXLINE);
        writelog(buf);
    }
    return;
}

void post_requesthdrs(rio_t *rp,int *length)
{
    char buf[MAXLINE];
    char *p;
    Rio_readlineb(rp, buf, MAXLINE);
    writetime();  /* write access time in log file */
    while(strcmp(buf, "\r\n"))
    {
        Rio_readlineb(rp, buf, MAXLINE);
        if(strncasecmp(buf,"Content-Length:",15)==0)
        {
            p=&buf[15];
            p+=strspn(p," \t");
            *length= (int)atol(p);
        }
        writelog(buf);
    }
    return;
}


void serve_dir(int fd,char *filename)
{
    DIR *dp;
    struct dirent *dirp;
    struct stat sbuf;
    struct passwd *filepasswd;
    int num=1;
    char files[MAXLINE],buf[MAXLINE],name[MAXLINE],img[MAXLINE],modifyTime[MAXLINE],dir[MAXLINE];
    char *p;

    /*
    * Start get the dir
    * for example: /home/yihaibo/kerner/web/doc/dir -> dir[]="dir/";
    */
    p=strrchr(filename,'/');
    ++p;
    strcpy(dir,p);
    strcat(dir,"/");
    /* End get the dir */

    if((dp=opendir(filename))==NULL)
        syslog(LOG_ERR,"cannot open dir:%s",filename);

    sprintf(files, "<html><title>Dir Browser</title>");
    sprintf(files,"%s<style type=""text/css""> a:link{text-decoration:none;} </style>",files);
    sprintf(files, "%s<body bgcolor=""ffffff"" font-family=Arial color=#fff font-size=14px>\r\n", files);

    while((dirp=readdir(dp))!=NULL)
    {
        if(strcmp(dirp->d_name,".")==0||strcmp(dirp->d_name,"..")==0)
            continue;
        sprintf(name,"%s/%s",filename,dirp->d_name);
        Stat(name,&sbuf);
        filepasswd=getpwuid(sbuf.st_uid);

        if(S_ISDIR(sbuf.st_mode))
        {
            sprintf(img,"<img src=""dir.png"" width=""24px"" height=""24px"">");
        }
        else if(S_ISFIFO(sbuf.st_mode))
        {
            sprintf(img,"<img src=""fifo.png"" width=""24px"" height=""24px"">");
        }
        else if(S_ISLNK(sbuf.st_mode))
        {
            sprintf(img,"<img src=""link.png"" width=""24px"" height=""24px"">");
        }
        else if(S_ISSOCK(sbuf.st_mode))
        {
            sprintf(img,"<img src=""sock.png"" width=""24px"" height=""24px"">");
        }
        else
            sprintf(img,"<img src=""file.png"" width=""24px"" height=""24px"">");


        sprintf(files,"%s<p><pre>%-2d %s ""<a href=%s%s"">%-15s</a>%-10s%10d %24s</pre></p>\r\n",files,num++,img,dir,dirp->d_name,dirp->d_name,filepasswd->pw_name,(int)sbuf.st_size,timeModify(sbuf.st_mtime,modifyTime));
    }
    closedir(dp);
    sprintf(files,"%s</body></html>",files);

    /* Send response headers to client */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, strlen(files));
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, "text/html");

#ifdef HTTPS
    if(ishttps)
    {
        SSL_write(ssl,buf,strlen(buf));
        SSL_write(ssl,files,strlen(files));
    }
    else
#endif
    {
        Rio_writen(fd, buf, strlen(buf));
        Rio_writen(fd, files, strlen(files));
    }
#ifdef HTTPS
    if(ishttps)
        exit(0);
#endif
}



/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;
//	char tmpcwd[MAXLINE];
    //strcpy(tmpcwd,cwd);
    //strcat(tmpcwd,"/");
//    printf("before parse,uri:%s\t filename:%s\n",uri,filename);
    strncpy(filename, http_root_dir,strlen(http_root_dir));
    if (!strstr(uri, "cgi-bin"))   /* Static content */
    {
        strcpy(cgiargs, "");
        strcat(filename, uri);
        if (uri[strlen(uri)-1] == '/')
            strcat(filename, "home.html");

//        printf("filename:%s\n",filename);
//        printf("after parse,uri:%s\t filename:%s\n",uri,filename);
        return 1;
    }
    else /* Dynamic content */
    {
        ptr = index(uri, '?'); // get request
        if (ptr)
        {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        }
        else
        {
            strcpy(cgiargs, "");
        }
        strcat(filename, uri);
//        printf("filename:%s\n",filename);
//        printf("after parse,uri:%s\t filename:%s\n",uri,filename);
        return 0;
    }
}

/*
 * serve_static - copy a file back to the client
 */
void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

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

#ifdef HTTPS
    if(ishttps)
    {
        SSL_write(ssl, buf, strlen(buf));
        SSL_write(ssl, srcp, filesize);
    }
    else
#endif
    {
        Rio_writen(fd, buf, strlen(buf));  // send start line and the reponse header
        Rio_writen(fd, srcp, filesize);    // send the reponse body
    }
    Munmap(srcp, filesize);
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(const char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else
        strcpy(filetype, "text/plain");
}

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
void get_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE], *emptylist[] = { NULL },httpsbuf[MAXLINE];
    int p[2];

#ifdef HTTPS
    if(ishttps)
    {
        Pipe(p);
        if (Fork() == 0)
        {  /* child  */
            Close(p[0]);
            setenv("QUERY_STRING", cgiargs, 1);
            Dup2(p[1], STDOUT_FILENO);         /* Redirect stdout to p[1] */
            Execve(filename, emptylist, environ); /* Run CGI program */
        }
        Close(p[1]);
        Read(p[0],httpsbuf,MAXLINE);   /* parent read from p[0] */
        SSL_write(ssl,httpsbuf,strlen(httpsbuf));
    }
    else
#endif
    {
        if (Fork() == 0)
        { /* child */
            /* Real server would set all CGI vars here */
            setenv("QUERY_STRING", cgiargs, 1);
            Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */
            Execve(filename, emptylist, environ); /* Run CGI program */
        }
    }
    //Wait(NULL); /* Parent waits for and reaps child */
    //todo
}

void post_dynamic(int fd, char *filename, int contentLength,rio_t *rp)
{
    char buf[MAXLINE],length[32], *emptylist[] = { NULL },data[MAXLINE];
    int p[2];
    memset(data,0,MAXLINE);
    memset(buf,0,MAXLINE);
    memset(length,0,32);
#ifdef HTTPS
    int httpsp[2];
#endif
    sprintf(length,"%d",contentLength);
//    printf("length:%s\n",length);
    Pipe(p);

    /*   The post data is sended by client,we need to redirct the data to cgi stdin.
    *  	 so, child read contentLength bytes data from fp,and write to p[1];
    *    parent should redirct p[0] to stdin. As a result, the cgi script can
    *    read the post data from the stdin.
    */
    /* https already read all data ,include post data  by SSL_read() */
    // todo
    if (Fork() == 0)
    {                     /* child  */
        Close(p[0]);
#ifdef HTTPS
        if(ishttps)
        {
            Write(p[1],httpspostdata,contentLength);
        }
        else
#endif
        {
            Rio_readnb(rp,data,contentLength);
//            printf("data in post before sent to pipe in http:%s\n",data);
            Rio_writen(p[1],data,contentLength);
        }
        exit(0);
    }

    //Wait(NULL);
    Dup2(p[0],STDIN_FILENO);  /* Redirct p[0] to stdin */
    Close(p[0]);
    Close(p[1]);
//    setenv("CONTENT-LENGTH",length , 1);

#ifdef HTTPS
    if(ishttps)  /* if ishttps,we couldnot redirct stdout to client,we must use SSL_write */
    {
        Pipe(httpsp);

        if(Fork()==0)
        {
            setenv("CONTENT-LENGTH",length , 1);
            Dup2(httpsp[1],STDOUT_FILENO);        /* Redirct stdout to https[1] */
            Execve(filename, emptylist, environ);
        }
        //Wait(NULL);
//        printf("test here,https[0] = %d\n",httpsp[0]);
        Read(httpsp[0],data,MAXLINE);
//        printf("filename:%s\n",filename);
//        printf("data:%s\n",data);
        SSL_write(ssl,data,strlen(data));
    }
    else
#endif
    {
        if(Fork() == 0) {
            setenv("CONTENT-LENGTH",length , 1);
            Dup2(fd, STDOUT_FILENO);        /* Redirct stdout to client */
            Execve(filename, emptylist, environ);
        }
    }
}

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
//	sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    sprintf(buf, "%sContent-type: text/html\r\n",buf);
    sprintf(buf, "%sContent-length: %d\r\n\r\n",buf,(int)strlen(body));

#ifdef HTTPS
    if(ishttps)
    {
        SSL_write(ssl,buf,strlen(buf));
        SSL_write(ssl,body,strlen(body));
    }
    else
#endif
    {
        Rio_writen(fd, buf, strlen(buf));
        Rio_writen(fd, body, strlen(body));
    }
}


/* if the process is running, the interger in the pid file is the pid, else is -1  */
void writePid(int option)
{
    int pid;
    FILE *fp=Fopen(PID_FILE,"w+");
    assert(fp != NULL);
    if(option)
        pid=(int)getpid();
    else
        pid=-1;
    fprintf(fp,"%d",pid);
    Fclose(fp);
}
