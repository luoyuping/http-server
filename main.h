//
// Created by luo on 16-5-15.
//
#include "wrap.h"
#include "parse.h"
#ifndef _MAIN_H
#define _MAIN_H

extern int isShowdir;
extern char* cwd;

#ifdef  HTTPS
extern SSL_CTX* ssl_ctx;
extern SSL* ssl;
extern char* certfile;
extern int ishttps;
extern char httpspostdata[MAXLINE];
extern char http_root_dir[MAXLINE];
#endif

void doit(int fd);
void writePid(int option);
void get_requesthdrs(rio_t *rp);
void post_requesthdrs(rio_t *rp,int *length);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void serve_dir(int fd,char *filename);
void get_filetype(const char *filename, char *filetype);
void get_dynamic(int fd, char *filename, char *cgiargs);
void post_dynamic(int fd, char *filename, int contentLength,rio_t *rp);
void clienterror(int fd, char *cause, char *errnum,
                        char *shortmsg, char *longmsg);

void sigChldHandler(int signo);
/* $begin ssl */
#ifdef HTTPS
void ssl_init(void);
void https_getlength(char* buf,int* length);
#endif

/* $end ssl */
#endif //_MAIN_H
