/* vi: set tabstop=4 nocindent noautoindent: */

#ifndef _FESERVER_H
#define _FESERVER_H


#include "tools.h"
#include "buffer.h"
#include "tlist.h"
#include "network.h"
#include "ssl_tool.h"
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <errno.h>


//extern char*  ServerIPaddr;
//extern char*  ClientIPaddr;
//extern char*  MyIPaddr;
//extern char*  MyNetworkaddr;
//extern char*  ClientName;

//extern unsigned char*  ServerIPaddr_num;
//extern unsigned char*  ClientIPaddr_num;
//extern unsigned char*  MyIPaddr_num;
//extern unsigned char*  MyNetworkaddr_num;


#define  NO_SYSLOG  999


// 以上は feplg.h と同じ内容にすること．





///////////////////////////////////////////////////////////////////////////////////

#define  TIME_OUT   600


int   main(int argc, char** argv);
void  receipt(char* hostname, int cport, struct sockaddr addr, SSL_CTX* s_ctx, SSL_CTX* c_ctx);
void  sig_term(int signal);
void  sig_child(int signal);

int   load_module(char*);
void* load_function(void*, char*);



///////////////////////////////////////////////////////////////////////////////////

int  (*init_main)(int, tList*);
int  (*term_main)(void);
int  (*init_process)(int, char*);
int  (*term_process)(int);
int  (*fe_server)(int nofd, int cofd, SSL* sssl, SSL* cssl, char* msg, int cc);     // S -> C   fesrv is C
int  (*fe_client)(int sofd, int nofd, SSL* cssl, SSL* sssl, char* msg, int cc);     // C -> S   fesrv is S



#endif

