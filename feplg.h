
#ifndef _FEPLUG_H
#define _FEPLUG_H


#include "tools.h"
#include "buffer.h"
#include "tlist.h"
#include "network.h"
#include "ssl_tool.h"
#include "ipaddr_tool.h"
#include "protocol.h"

#include <syslog.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <errno.h>


#define  NO_SYSLOG  999


/////////////////////////////////////////////////////////////////////////////////////////////

int  fe_server(int csofd, int nsofd, SSL* cssl, SSL* sssl, char* msg, int cc);  // S -> C   fesrv is C
int  fe_client(int nsofd, int csofd, SSL* sssl, SSL* cssl, char* msg, int cc);  // C -> S   fesrv is S
int  init_main(int mode, tList* file);
int  term_main(void);
int  init_process(int, char*);
int  term_process(int);

/////////////////////////////////////////////////////////////////////////////////////////////


#endif

